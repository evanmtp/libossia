// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>


#include <ossia-max/src/ossia-max.hpp>
#include <ossia-max/src/utils.hpp>
#include <ossia/context.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>

#include <commonsyms.h>
#pragma mark -
#pragma mark library

#include <git_info.h>

using namespace ossia::max;

void* ossia_max::browse_clock;
ZeroconfOscqueryListener ossia_max::zeroconf_oscq_listener;
ZeroconfMinuitListener ossia_max::zeroconf_minuit_listener;

// ossia-max library constructor
ossia_max::ossia_max():
    m_localProtocol{new ossia::net::local_protocol},
    m_device{std::unique_ptr<ossia::net::protocol_base>(m_localProtocol), "ossia_max_device"},
    m_log_sink{std::make_shared<max_msp_log_sink>()}
{
  m_log_sink.get()->set_level(spdlog::level::err);
  ossia::context c{{m_log_sink}};
  common_symbols_init();

  m_device.on_attribute_modified.connect<&device_base::on_attribute_modified_callback>();

  parameters.reserve(2048);
  remotes.reserve(1024);
  attributes.reserve(512);
  models.reserve(512);
  views.reserve(512);
  devices.reserve(8);
  clients.reserve(8);

#if OSSIA_MAX_AUTOREGISTER
  m_reg_clock = clock_new(this, (method) ossia_max::register_nodes);
#endif

  m_timer_clock = clock_new(this, (method) ossia_max::poll_all_queues);
  browse_clock = clock_new(this, (method) ossia_max::discover_network_devices);
  clock_delay(ossia_max::browse_clock, 100.);

  post("OSSIA library for Max is loaded");
  post("build SHA : %s", ossia::get_commit_sha().c_str());
}

// ossia-max library destructor
ossia_max::~ossia_max()
{
  m_device.on_attribute_modified.disconnect<&device_base::on_attribute_modified_callback>();

  for (auto x : devices.copy())
  {
    auto& multiplex = static_cast<ossia::net::multiplex_protocol&>(
                x->m_device->get_protocol());
    auto& protos = multiplex.get_protocols();
    for (auto& proto : protos)
    {
      multiplex.stop_expose_to(*proto);
    }
    x->m_protocols.clear();
    x->disconnect_slots();
  }
  for (auto x : views.copy()){
    x->m_matchers.clear();
  }
  for (auto x : remotes.copy()){
    x->m_matchers.clear();
  }
  for (auto x : models.copy()){
    x->m_matchers.clear();
  }
  for (auto x : parameters.copy()){
    x->m_matchers.clear();
  }
}

// ossia-max library instance
ossia_max& ossia_max::instance()
{
  static ossia_max library_instance;
  return library_instance;
}

template<typename T>
void fill_nr_vector(const ossia::safe_vector<T*>& safe, ossia::safe_set<T*>& nr)
{
  nr.clear();
  nr.reserve(safe.size());
  for(auto ptr : safe.reference())
    nr.push_back(ptr);
};

template<typename T>
std::vector<T*> sort_by_depth(const ossia::safe_set<T*>& safe)
{
  std::vector<T*> list;
  list.reserve(safe.size());
  for(auto pt : safe.reference())
  {
    // need to update hierarchy here because
    // some object might have been inserted
    // after the initialization of some of their children
    // thus children have the wrong hierarchy, so update it
    // before sorting them
    pt->get_hierarchy();
    list.push_back(pt);
  }

  ossia::sort(list, [&](T* a, T*b)
  {
    return a->m_patcher_hierarchy.size() < b->m_patcher_hierarchy.size();
  });

  return list;
}

void ossia_max::register_nodes(ossia_max* x)
{
  auto& inst = ossia_max::instance();

  inst.registering_nodes = true;

  // first fill non-registered containers with all objects
  fill_nr_vector(inst.devices, inst.nr_devices);
  fill_nr_vector(inst.models, inst.nr_models);
  fill_nr_vector(inst.parameters, inst.nr_parameters);
  fill_nr_vector(inst.clients,    inst.nr_clients);
  fill_nr_vector(inst.views, inst.nr_views);
  fill_nr_vector(inst.remotes, inst.nr_remotes);
  fill_nr_vector(inst.attributes, inst.nr_attributes);
  auto dev_obj_list   = sort_by_depth(inst.nr_devices);
  auto mod_obj_list   = sort_by_depth(inst.nr_models);
  auto param_obj_list = sort_by_depth(inst.nr_parameters);
  auto clt_obj_list = sort_by_depth(inst.nr_clients);
  auto view_obj_list = sort_by_depth(inst.nr_views);
  auto rem_obj_list = sort_by_depth(inst.nr_remotes);
  auto att_obj_list = sort_by_depth(inst.nr_attributes);

  std::vector<t_object*> to_be_initialized;

  auto& map = inst.root_patcher;
  for (auto it = map.begin(); it != map.end(); it++)
  {
    if(it->second.is_loadbanged)
      continue;

    t_object* patcher = it->first;

    to_be_initialized.push_back(patcher);

    for (auto dev : dev_obj_list)
    {
      if (dev->m_patcher_hierarchy.empty()) continue;
      if(dev->m_patcher_hierarchy.back() == patcher)
        ossia::max::device::register_children(dev);
    }
    for (auto model : mod_obj_list)
    {
      if (model->m_patcher_hierarchy.empty()) continue;
      if ( model->m_patcher_hierarchy.back() == patcher
            && model->m_matchers.empty())
        ossia_register(model);
    }
    for (auto param : param_obj_list)
    {
      if (param->m_patcher_hierarchy.empty()) continue;
      if ( param->m_patcher_hierarchy.back() == patcher
            && param->m_matchers.empty())
        ossia_register(param);
    }

    for (auto client : clt_obj_list)
    {
      if(client->m_patcher_hierarchy.empty()) continue;
      if(client->m_patcher_hierarchy.back() == patcher)
        ossia::max::client::register_children(client);
    }
    for (auto view : view_obj_list)
    {
      if (view->m_patcher_hierarchy.empty()) continue;
      if ( view->m_patcher_hierarchy.back() == patcher
            && view->m_matchers.empty())
        ossia_register(view);
    }
    for (auto remote : rem_obj_list)
    {
      if (remote->m_patcher_hierarchy.empty()) continue;
      if ( remote->m_patcher_hierarchy.back() == patcher
            && remote->m_matchers.empty())
        ossia_register(remote);
    }
    for (auto attr : att_obj_list)
    {
      if (attr->m_patcher_hierarchy.empty()) continue;
      if ( attr->m_patcher_hierarchy.back() == patcher
            && attr->m_matchers.empty())
        ossia_register(attr);
    }

    // finally rise a flag to mark this patcher loadbangded
    it->second.is_loadbanged = true;
  }

  inst.registering_nodes = false;

  // push default value for all devices
  std::vector<ossia::net::generic_device*> dev_list;
  dev_list.reserve(inst.devices.size() + 1);
  for(auto dev : inst.devices.reference())
  {
    dev_list.push_back(dev->m_device);
  }
  dev_list.push_back(inst.get_default_device());

  ossia::sort(dev_list, [&](ossia::net::generic_device* a, ossia::net::generic_device* b)
  {
    auto prio_a = ossia::net::get_priority(a->get_root_node());
    auto prio_b = ossia::net::get_priority(b->get_root_node());

    if(!prio_a)
      prio_a = 0.;

    if(!prio_b)
      prio_b = 0.;

    return *prio_a > *prio_b;
  });

  for (auto dev : dev_list)
  {
    auto list = ossia::net::list_all_child(&dev->get_root_node());

    for (ossia::net::node_base* child : list)
    {
      if (auto param = child->get_parameter())
      {
        auto val = ossia::net::get_default_value(*child);
        if(val)
        {
          bool trig = false;
          for(auto param : ossia_max::instance().parameters.reference())
          {
            for (auto& m : param->m_matchers)
            {
              if ( m->get_node() == child )
              {
                auto op = static_cast<parameter*>(m->get_parent());
                auto patcher = op->m_patcher_hierarchy.back();
                if(ossia::contains(to_be_initialized,patcher))
                {
                  child->get_parameter()->push_value(*val);
                  trig = true;
                  break;
                }
              }
            }
            if(trig)
              break;
          }

          if(trig)
          {
            trig_output_value(child);
          }
        }
      }
    }
  }
}

void ossia_max::start_timers()
{
  auto& x = ossia_max::instance();
  clock_set(x.m_timer_clock, 1);
  x.m_clock_count++;
}

void ossia_max::stop_timers()
{
  auto& x = ossia_max::instance();
  if( x.m_clock_count > 0 )
  {
    x.m_clock_count--;
  }
  else
  {
    std::cout << "stop poll timers" << std::endl;
    clock_unset(x.m_timer_clock);
  }
}

void ossia_max::poll_all_queues(ossia_max* x)
{
  for(auto param : ossia_max::instance().parameters.reference())
  {
    for (auto& m : param->m_matchers)
    {
      m->output_value();
    }
  }

  for(auto remote : ossia_max::instance().remotes.reference())
  {
    for (auto& m : remote->m_matchers)
    {
      m->output_value();
    }
  }

  clock_delay(x->m_timer_clock, 10); // TODO add method to change rate
}

namespace ossia
{
namespace max
{

template <typename T>
void object_quarantining(T* x)
{
  x->m_node_selection.clear();
  if (!object_is_quarantined<T>(x))
    x->quarantine().push_back(x);
}

template void object_quarantining<parameter>(parameter*);
template void object_quarantining<attribute>(attribute*);
template void object_quarantining<model>(model*);
template void object_quarantining<remote>(remote*);
template void object_quarantining<view>(view*);

template <typename T>
void object_dequarantining(T* x)
{
  x->quarantine().remove_all(x);
}

template void object_dequarantining<attribute>(attribute*);
template void object_dequarantining<parameter>(parameter*);
template void object_dequarantining<model>(model*);
template void object_dequarantining<remote>(remote*);
template void object_dequarantining<view>(view*);

template <typename T>
bool object_is_quarantined(T* x)
{
  return x->quarantine().contains(x);
}

template bool object_is_quarantined<parameter>(parameter*);
template bool object_is_quarantined<model>(model*);
template bool object_is_quarantined<remote>(remote*);
template bool object_is_quarantined<view>(view*);

#pragma mark -
#pragma mark Utilities

void register_quarantinized()
{
  for (auto model : model::quarantine().copy())
  {
    ossia_register(model);
  }

  for (auto parameter : parameter::quarantine().copy())
  {
    ossia_register(parameter);
  }

  for (auto view : view::quarantine().copy())
  {
    ossia_register(view);
  }

  for (auto remote : remote::quarantine().copy())
  {
    ossia_register(remote);
  }
}

std::vector<object_base*> find_children_to_register(
    t_object* caller, t_object* root_patcher, t_symbol* search_symbol, bool search_dev)
{
  t_symbol* subclassname = search_symbol == gensym("ossia.model")
                               ? gensym("ossia.parameter")
                               : gensym("ossia.remote");

  std::vector<object_base*> found;
  bool found_model = false;
  bool found_view = false;

  // 1: look for [classname] objects into the patcher
  t_object* next_box = object_attr_getobj(root_patcher, _sym_firstobject);

  t_object* object_box = NULL;
  object_obex_lookup(caller, gensym("#B"), &object_box);

  while (next_box)
  {
    if(next_box != object_box)
    {
      t_symbol* curr_classname = object_attr_getsym(next_box, _sym_maxclass);
      if (curr_classname == search_symbol)
      {
          object_base* o = (object_base*) jbox_get_object(next_box);

          // ignore dying object
          if (!o->m_dead)
            found.push_back(o);

      }

      // if we're looking for ossia.view but found a model, remind it
      if ( search_symbol == gensym("ossia.view")
           && curr_classname == gensym("ossia.model") )
        found_model = true;
      else if ( search_symbol == gensym("ossia.model")
                && curr_classname == gensym("ossia.view") )
        found_view = true;

      // if there is a client or device in the current patcher
      // don't register anything
      if ( search_dev
           && ( curr_classname == gensym("ossia.device")
             || curr_classname == gensym("ossia.client") ))
      {
        return {};
      }
    }
    next_box = object_attr_getobj(next_box, _sym_nextobject);
  }

  // 2: if there is no ossia.model / ossia.view in the current patch, look into
  // the subpatches
  if (found.empty())
  {
    next_box = object_attr_getobj(root_patcher, _sym_firstobject);

    while (next_box)
    {
      t_object* object = jbox_get_object(next_box);
      t_symbol* classname = object_classname(object);

      // jpatcher or bpatcher case
      if (classname == _sym_jpatcher
          || classname == _sym_bpatcher)
      {
        std::vector<object_base*> found_tmp
            = find_children_to_register(caller, object, search_symbol, true);

        found.insert(found.end(),found_tmp.begin(), found_tmp.end());

      }
      else if (classname == gensym("poly~"))
      {
        long idx = 0;
        t_object* subpatcher = (t_object*)object_method(object, gensym("subpatcher"), idx++, 0);
        while(subpatcher)
        {
            std::vector<object_base*> found_tmp
                = find_children_to_register(caller, subpatcher, search_symbol, true);

            found.insert(found.end(),found_tmp.begin(), found_tmp.end());

            subpatcher = (t_object*)object_method(object, gensym("subpatcher"), idx++, 0);
        }
      }

      next_box = object_attr_getobj(next_box, _sym_nextobject);
    }

    // 3: finally look for ossia.param / ossia.remote in the same pather
    next_box = object_attr_getobj(root_patcher, _sym_firstobject);

    while (next_box)
    {
      if (object_box != next_box)
      {
        t_symbol* current = object_attr_getsym(next_box, _sym_maxclass);
        if (current == subclassname
            || ( !found_view && current == gensym("ossia.remote") ) )
        {

          // the object itself shouln't be stored
          if (object_box != next_box)
          {
            object_base* o = (object_base*) jbox_get_object(next_box);
            found.push_back(o);
          }
        }
      }
      next_box = object_attr_getobj(next_box, _sym_nextobject);
    }
  }

  return found;
}

t_object* get_patcher(t_object* object)
{
  t_object* patcher = nullptr;
  auto err = object_obex_lookup(object, gensym("#P"), &patcher);

  auto bpatcher = object_attr_getobj(object, _sym_parentpatcher);

  if(patcher != nullptr && err == MAX_ERR_NONE)
    return patcher;
  else
    return bpatcher;
}

std::vector<std::string> parse_tags_symbol(t_symbol** tags_symbol, long size)
{
  std::vector<std::string> tags;

  for(int i=0;i<size;i++)
  {
    tags.push_back(tags_symbol[i]->s_name);
  }

  return tags;
}

void ossia_max::discover_network_devices(ossia_max* x)
{
  ossia_max::zeroconf_oscq_listener.browse();
  ossia_max::zeroconf_minuit_listener.browse();
  clock_delay(ossia_max::browse_clock, 100.);
}

} // max namespace
} // ossia namespace
