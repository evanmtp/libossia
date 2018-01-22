#pragma once
#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/apply.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/state/flat_state.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <ossia/network/value/value_algorithms.hpp>
#include <ossia/network/value/value_traits.hpp>
#include <ossia/network/base/parameter.hpp>
#include <iostream>
namespace ossia
{

static inline OSSIA_DECL_RELAXED_CONSTEXPR bool is_vec(ossia::val_type v)
{
  switch (v)
  {
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return true;
    default:
      return false;
  }
}

inline bool same_vec_type(const ossia::value& lhs, const ossia::value& rhs)
{
  const auto first = lhs.getType();
  const auto second = rhs.getType();
  if (first != second)
    return false;

  return is_vec(first);
}

struct vec_merger
{
  const ossia::destination& existing_dest;
  const ossia::destination& incoming_dest;

  template <typename T, typename U>
  ossia::state_element operator()(T&, const U&) const
  {
    // Invalid cases.
    // TODO Do nothing, or throw ?
    // throw std::runtime_error("vec_merger: invalid case");
    return {};
  }
  template <typename T, typename U>
  ossia::state_element operator()(const T&, const U&) const
  {
    // Invalid cases.
    // TODO Do nothing, or throw ?
    // throw std::runtime_error("vec_merger: invalid case");
    return {};
  }

  template <std::size_t N>
  auto make_piecewise_from_floats(float orig, float incoming) const
  {
    piecewise_vec_message<N> mess{
        existing_dest.value, {}, incoming_dest.unit, {}};

    auto& existing_index = existing_dest.index;
    if (existing_index[0] < (int64_t)N)
    {
      mess.message_value[existing_index[0]] = orig;
      mess.used_values.set(existing_index[0]);
    }

    auto& incoming_index = incoming_dest.index;
    if (incoming_index[0] < (int64_t)N)
    {
      mess.message_value[incoming_index[0]] = incoming;
      mess.used_values.set(incoming_index[0]);
    }
    return mess;
  }

  ossia::state_element operator()(float orig, float incoming) const
  {
    auto& existing_index = existing_dest.index;
    auto& incoming_index = incoming_dest.index;

    if (!existing_index.empty() && !incoming_index.empty()
        && existing_index != incoming_index)
    {
      ossia::val_type type;
      if (incoming_dest.unit)
        type = ossia::matching_type(incoming_dest.unit);
      else
        type = existing_dest.address().get_value_type();

      switch (type)
      {
        case ossia::val_type::VEC2F:
          return make_piecewise_from_floats<2>(orig, incoming);
        case ossia::val_type::VEC3F:
          return make_piecewise_from_floats<3>(orig, incoming);
        case ossia::val_type::VEC4F:
          return make_piecewise_from_floats<4>(orig, incoming);
        default:
          break;
      }
    }

    return ossia::message{incoming_dest, incoming};
  }

  template <std::size_t N>
  ossia::state_element
  operator()(std::array<float, N>& orig, float incoming) const
  {
    auto& existing_index = existing_dest.index;
    auto& incoming_index = incoming_dest.index;
    if (incoming_index.empty())
    {
      return {}; // TODO or maybe put it at index zero ... ?
    }
    else
    {
      auto i = incoming_index[0];
      if (i < (int64_t)N)
        orig[i] = incoming;

      if (existing_index != incoming_index && !existing_index.empty())
      {
        // Case where we had a message setting the index [0] and another
        // setting the index [2]
        // for instance
        piecewise_vec_message<N> mess{
            existing_dest.value, orig, incoming_dest.unit, {}};
        mess.used_values.set(existing_index[0]);
        mess.used_values.set(i);
        return mess;
      }
      else
      {
        return {};
      }
    }
  }

  template <std::size_t N>
  ossia::state_element operator()(
      std::array<float, N>& orig, const std::array<float, N>& incoming) const
  {
    auto& existing_index = existing_dest.index;
    auto& incoming_index = incoming_dest.index;
    if (incoming_index.empty())
    {
      orig = incoming;
      return {};
    }
    else
    {
      auto i = incoming_index[0];
      if (i < (int64_t)N)
        orig[i] = incoming[i];

      if (existing_index != incoming_index && !existing_index.empty())
      {
        // Case where we had a message setting the index [0] and another
        // setting the index [2]
        // for instance
        piecewise_vec_message<N> mess{
            existing_dest.value, orig, incoming_dest.unit, {}};
        mess.used_values.set(existing_index[0]);
        mess.used_values.set(i);
        return mess;
      }
      else
      {
        return {};
      }
    }
  }

  template <std::size_t N>
  ossia::state_element operator()(
      std::array<float, N>& orig,
      const std::vector<ossia::value>& incoming) const
  {
    auto& existing_index = existing_dest.index;
    auto& incoming_index = incoming_dest.index;
    if (incoming_index.empty())
    {
      value_merger<true>::merge_list(orig, incoming);
      return {};
    }
    else
    {
      auto i = incoming_index[0];
      if (i < (int64_t)N)
      {
        value_merger<true>::write_float(incoming[i], orig[i]);
      }

      if (existing_index != incoming_index && !existing_index.empty())
      {
        // Case where we had a message setting the index [0] and another
        // setting the index [2]
        // for instance
        piecewise_vec_message<N> mess{
            existing_dest.value, orig, incoming_dest.unit, {}};
        mess.used_values.set(existing_index[0]);
        mess.used_values.set(i);
        return mess;
      }
      else
      {
        return {};
      }
    }
  }
};

template<typename State_T, typename ExistingIt, bool MergeSingleValues>
struct state_flatten_visitor_merger
{
  State_T& state;
  ExistingIt existing_it;

  //// Const reference overloads
  //// Message incoming
  void operator()(message& existing, const message& incoming)
  {
    auto to_append_index_empty = incoming.dest.index.empty();
    auto source_index_empty = existing.dest.index.empty();
    if (incoming.message_value.valid()
        && (same_vec_type(existing.message_value, incoming.message_value)
            || is_vec(existing.dest.address().get_value_type())))
    {
      // We handle the Vec types a bit differently :
      // since it's very cheap, the value will contain the whole array data
      // and the index will be the relevant index in the array.
      // Hence we merge both indexes.
      auto res = ossia::apply(
          vec_merger{existing.dest, incoming.dest},
          existing.message_value.v, incoming.message_value.v);

      if (res)
      {
        state.remove(existing_it);
        state.add(res);
      }
    }
    else if (to_append_index_empty && source_index_empty)
    {
      if(MergeSingleValues)
      {
        // Replace existing values
        value_merger<true>::merge_value(
                    existing.message_value, incoming.message_value);
      }
      else
      {
        // Add the new message to the state
        state.add(incoming);
      }
    }
    else
    {
      piecewise_message pw{
          incoming.dest.value, {}, incoming.dest.unit};
      if (!to_append_index_empty && !source_index_empty)
      {
        // Most complex case : we create a list big enough to host both values
        value_merger<true>::insert_in_list(
            pw.message_value, existing.message_value,
            existing.dest.index);
        value_merger<true>::insert_in_list(
            pw.message_value, incoming.message_value,
            incoming.dest.index);
      }
      // For these cases, we mix in the one that has the index;
      // the one without index information becomes index [0] :
      else if (!to_append_index_empty)
      {
        pw.message_value.push_back(existing.message_value);
        value_merger<true>::insert_in_list(
            pw.message_value, incoming.message_value,
            incoming.dest.index);
      }
      else if (!source_index_empty)
      {
        value_merger<true>::insert_in_list(
            pw.message_value, existing.message_value,
            existing.dest.index);
        value_merger<true>::set_first_value(
            pw.message_value, incoming.message_value);
      }
      state.remove(existing_it);
      state.add(std::move(pw));
    }
  }

  void operator()(piecewise_message& existing, const message& incoming)
  {
    if (incoming.dest.index.empty())
    {
      // add it at [0]
      value_merger<true>::set_first_value(
          existing.message_value, incoming.message_value);
    }
    else
    {
      // add it wherever possible, by extending the list as required
      value_merger<true>::insert_in_list(
          existing.message_value, incoming.message_value,
          incoming.dest.index);
    }
  }

  template <std::size_t N>
  void operator()(piecewise_vec_message<N>& existing, const message& incoming)
  {
    auto t = incoming.message_value.getType();
    using vec_type = decltype(existing.message_value);
    switch (t)
    {
      // incoming is a Float with index
      case ossia::val_type::FLOAT:
      {
        if (!incoming.dest.index.empty())
        {
          auto i = incoming.dest.index[0];
          if (i <(int64_t) N)
          {
            existing.message_value[i] = incoming.message_value.get<float>();
            existing.used_values.set(i);
          }
        }
        break;
      }
      // incoming is a Vec<N, float> without index or with index
      case ossia::value_trait<vec_type>::ossia_enum:
      {
        // With index -> set it
        if (!incoming.dest.index.empty())
        {
          auto i = incoming.dest.index[0];
          if (i <(int64_t) N)
          {
            auto& inc = incoming.message_value.get<vec_type>();
            existing.message_value[i] = inc[i];
            existing.used_values.set(i);
          }
        }
        // Without index -> replace everything
        else
        {
          existing.message_value = incoming.message_value.get<vec_type>();
        }
        break;
      }
      default:
        break;
    }
  }

  //// Piecewise Message incoming
  void operator()(message& existing, const piecewise_message& incoming)
  {
    piecewise_message other = incoming;
    // Merge the message in the piecewise_message and replace it
    // If the message refers to an index in the piecewise_message we merge it
    // unless the piecewise_message also has a
    // value at this index

    if (existing.dest.index.empty())
    {
      // add it at [0]
      value_merger<false>::set_first_value(
          other.message_value, existing.message_value);
    }
    else
    {
      // add it wherever possible, by extending the list as required
      value_merger<false>::insert_in_list(
          other.message_value, existing.message_value,
          existing.dest.index);
    }

    state.remove(existing_it);
    state.remove(incoming);
    state.add(std::move(other));
  }

  void
  operator()(piecewise_message& existing, const piecewise_message& incoming)
  {
    // merge the new piecewise into the existing one
    value_merger<true>::merge_list(
        existing.message_value, incoming.message_value);
  }

  //// Piecewise Vec Message incoming
  template <std::size_t N>
  void operator()(
      piecewise_vec_message<N>& existing,
      const piecewise_vec_message<N>& incoming)
  {
    for (std::size_t i = 0; i < N; i++)
    {
      if (incoming.used_values.test(i))
      {
        existing.message_value[i] = incoming.message_value[i];
        existing.used_values.set(i);
      }
    }
  }

  //// R-value reference overloads
  //// Message incoming
  void operator()(message& existing, message&& incoming)
  {
    //    std::cerr << ossia::to_pretty_string(existing.destination) << " : "
    //              << ossia::value_to_pretty_string(existing) << " <= "
    //              << ossia::to_pretty_string(incoming.destination) << " : "
    //              << ossia::value_to_pretty_string(incoming) << std::endl;

    auto to_append_index_empty = incoming.dest.index.empty();
    auto source_index_empty = existing.dest.index.empty();
    if (incoming.message_value.valid()
        && (same_vec_type(existing.message_value, incoming.message_value)
            || is_vec(existing.dest.address().get_value_type())))
    {
      // We handle the Vec types a bit differently :
      // since it's very cheap, the value will contain the whole array data
      // and the index will be the relevant index in the array.
      // Hence we merge both indexes.
      auto res = ossia::apply(
          vec_merger{existing.dest, incoming.dest},
          existing.message_value.v, incoming.message_value.v);

      if (res)
      {
        state.remove(existing_it);
        state.add(std::move(res));
      }
    }
    else if (to_append_index_empty && source_index_empty)
    {
      // Add the new message to the state
      if(MergeSingleValues)
      {
        value_merger<true>::merge_value(
            existing.message_value, std::move(incoming.message_value));
      }
      else
      {
        state.add(std::move(incoming));
      }
    }
    else
    {
      piecewise_message pw{
          incoming.dest.value, {}, incoming.dest.unit};
      if (!to_append_index_empty && !source_index_empty)
      {
        // Most complex case : we create a list big enough to host both values
        value_merger<true>::insert_in_list(
            pw.message_value, existing.message_value,
            existing.dest.index);
        value_merger<true>::insert_in_list(
            pw.message_value, std::move(incoming.message_value),
            incoming.dest.index);
      }
      // For these cases, we mix in the one that has the index;
      // the one without index information becomes index [0] :
      else if (!to_append_index_empty)
      {
        pw.message_value.push_back(existing.message_value);
        value_merger<true>::insert_in_list(
            pw.message_value, std::move(incoming.message_value),
            incoming.dest.index);
      }
      else if (!source_index_empty)
      {
        value_merger<true>::insert_in_list(
            pw.message_value, existing.message_value,
            existing.dest.index);
        value_merger<true>::set_first_value(
            pw.message_value, std::move(incoming.message_value));
      }

      state.remove(existing_it);
      state.add(std::move(pw));
    }
  }
  void operator()(piecewise_message& existing, message&& incoming)
  {
    if (incoming.dest.index.empty())
    {
      // add it at [0]
      value_merger<true>::set_first_value(
          existing.message_value, std::move(incoming.message_value));
    }
    else
    {
      // add it wherever possible, by extending the list as required
      value_merger<true>::insert_in_list(
          existing.message_value, std::move(incoming.message_value),
          incoming.dest.index);
    }
  }

  //// Piecewise Message incoming
  void operator()(message& existing, piecewise_message&& incoming)
  {
    piecewise_message other = incoming;
    // Merge the message in the piecewise_message and replace it
    // If the message refers to an index in the piecewise_message we merge it
    // unless the piecewise_message also has a
    // value at this index

    if (existing.dest.index.empty())
    {
      // add it at [0]
      value_merger<false>::set_first_value(
          other.message_value, existing.message_value);
    }
    else
    {
      // add it wherever possible, by extending the list as required
      value_merger<false>::insert_in_list(
          other.message_value, existing.message_value,
          existing.dest.index);
    }

    state.remove(existing_it);
    state.remove(incoming);
    state.add(std::move(other));
  }

  void operator()(piecewise_message& existing, piecewise_message&& incoming)
  {
    // merge the new piecewise into the existing one
    value_merger<true>::merge_list(
        existing.message_value, std::move(incoming.message_value));
  }

  //// Piecewise Vec Message incoming : we don't have rvalue overloads since
  //// it's faster to copy.

  //// Error cases:
  template <typename T>
  void operator()(ossia::state&, const T&)
  {
    throw std::runtime_error(
        "state_flatten_visitor_merger: "
        "impossible case (state <- *");
  }

  template <std::size_t M>
  void operator()(message& existing, const piecewise_vec_message<M>& incoming)
  {
    throw std::runtime_error(
        "state_flatten_visitor_merger: "
        "impossible case (message <- const piecewise_vec_message&");
  }

  template <std::size_t M>
  void operator()(
      piecewise_message& existing, const piecewise_vec_message<M>& incoming)
  {
    throw std::runtime_error(
        "state_flatten_visitor_merger: "
        "impossible case (piecewise_message <- const piecewise_vec_message&");
  }

  template <std::size_t N>
  void operator()(
      piecewise_vec_message<N>& existing, const piecewise_message& incoming)
  {
    throw std::runtime_error(
        "state_flatten_visitor_merger: "
        "impossible case (piecewise_vec_message <- const piecewise_message&");
  }
  template <std::size_t N>
  void
  operator()(piecewise_vec_message<N>& existing, piecewise_message&& incoming)
  {
    throw std::runtime_error(
        "state_flatten_visitor_merger: "
        "impossible case (piecewise_vec_message <- piecewise_message&&");
  }

  template <std::size_t N, std::size_t M>
  void operator()(
      piecewise_vec_message<N>& existing,
      const piecewise_vec_message<M>& incoming)
  {
    throw std::runtime_error(
        "state_flatten_visitor_merger: "
        "impossible case (piecewise_vec_message<N> <- "
        "piecewise_vec_message<M>");
  }
};

template<typename State_T, bool MergeSingleValues, bool AssumeSameAddresses = false>
struct state_flatten_visitor
{
  State_T& state;

  static ossia::net::parameter_base* param_ptr(const message& m)
  {
    return &m.dest.value.get();
  }

  static ossia::net::parameter_base* param_ptr(const piecewise_message& m)
  {
    return &m.address.get();
  }

  template <std::size_t N>
  static ossia::net::parameter_base*
  param_ptr(const piecewise_vec_message<N>& m)
  {
    return &m.address.get();
  }

  template <typename State_U, typename T>
  static auto find_same_param(State_U& st, const T& incoming)
  {
    if constexpr(AssumeSameAddresses)
    {
      return find_if(st, [&](const state_element& e)
      {
        const auto tgt = e.target();
        switch(e.which())
        {
          case 0:
          {
            const auto m = static_cast<const message*>(tgt);
            return incoming.get_unit() == m->get_unit();
          }
            // 1 is state
          case 2:
          {
            const auto p = static_cast<const piecewise_message*>(tgt);
            return incoming.get_unit() == p->get_unit();
          }
          case 3:
          {
            const auto p = static_cast<const piecewise_vec_message<2>*>(tgt);
            return incoming.get_unit() == p->get_unit();
          }
          case 4:
          {
            const auto p = static_cast<const piecewise_vec_message<3>*>(tgt);
            return incoming.get_unit() == p->get_unit();
          }
          case 5:
          {
            const auto p = static_cast<const piecewise_vec_message<4>*>(tgt);
            return incoming.get_unit() == p->get_unit();
          }
          default:
            return false;
        }
      });
    }
    else
    {
      return find_if(st, [&](const state_element& e)
      {
        const auto address = param_ptr(incoming);
        const auto tgt = e.target();
        switch(e.which())
        {
          case 0:
          {
            const auto m = static_cast<const message*>(tgt);
            return &m->dest.value.get() == address && incoming.get_unit() == m->get_unit();
          }
            // 1 is state
          case 2:
          {
            const auto p = static_cast<const piecewise_message*>(tgt);
            return &p->address.get() == address && incoming.get_unit() == p->get_unit();
          }
          case 3:
          {
            const auto p = static_cast<const piecewise_vec_message<2>*>(tgt);
            return &p->address.get() == address && incoming.get_unit() == p->get_unit();
          }
          case 4:
          {
            const auto p = static_cast<const piecewise_vec_message<3>*>(tgt);
            return &p->address.get() == address && incoming.get_unit() == p->get_unit();
          }
          case 5:
          {
            const auto p = static_cast<const piecewise_vec_message<4>*>(tgt);
            return &p->address.get() == address && incoming.get_unit() == p->get_unit();
          }
          default:
            return false;
        }
      });
    }
  }

  template <typename T>
  static auto find_same_param(ossia::flat_set_state& st, const T& incoming)
  {
    return st.find(incoming);
  }

  template <typename T>
  static auto find_same_param(ossia::mono_state& st, const T& incoming)
  {
    return st.find(incoming);
  }

  ossia::state_element& value(ossia::state_element* iterator)
  {
    return *iterator;
  }
  ossia::state_element& value(std::vector<ossia::state_element>::iterator iterator)
  {
    return *iterator;
  }
  ossia::state_element& value(ossia::flat_vec_state::iterator iterator)
  {
    return *iterator;
  }
  ossia::state_element& value(ossia::flat_set_state::iterator iterator)
  {
    return iterator->second;
  }

  template <typename Message_T>
  void operator()(Message_T&& incoming)
  {
    // find message with the same address to replace it
    auto it = find_same_param(state, incoming);
    if (it == state.end())
    {
      state.add(std::forward<Message_T>(incoming));
    }
    else
    {
      using namespace eggs::variants;
      // Merge messages
      state_flatten_visitor_merger<State_T, std::remove_reference_t<decltype(it)>, MergeSingleValues> merger{state, it};
      // Workaround for a GDB bug if we use a generic lambda
      // in a template function (operator() here)
      switch (value(it).which())
      {
        case 0:
          merger(
              *value(it).template target<state_element_by_index<0>>(),
              std::forward<Message_T>(incoming));
          break;
        case 1:
          merger(
              *value(it).template target<state_element_by_index<1>>(),
              std::forward<Message_T>(incoming));
          break;
        case 2:
          merger(
              *value(it).template target<state_element_by_index<2>>(),
              std::forward<Message_T>(incoming));
          break;
        case 3:
          merger(
              *value(it).template target<state_element_by_index<3>>(),
              std::forward<Message_T>(incoming));
          break;
        case 4:
          merger(
              *value(it).template target<state_element_by_index<4>>(),
              std::forward<Message_T>(incoming));
          break;
        case 5:
          merger(
              *value(it).template target<state_element_by_index<5>>(),
              std::forward<Message_T>(incoming));
          break;
        default:
          break;
      }
    }
  }

  void operator()(const ossia::state& s)
  {
    state.reserve(state.size() + s.size());
    for (const auto& e : s)
    {
      ossia::apply(*this, e);
    }
  }

  void operator()(ossia::state&& s)
  {
    state.reserve(state.size() + s.size());
    for (auto&& e : s)
    {
      ossia::apply(*this, std::move(e));
    }
  }

  void operator()()
  {
  }
};
}
