#include <QtTest>
#include <ossia/ossia.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <iostream>
#include <thread>
#include <atomic>

using namespace ossia;
struct root_scenario
{
  std::shared_ptr<ossia::time_node> start_node{std::make_shared<ossia::time_node>()};
  std::shared_ptr<ossia::time_node> end_node{std::make_shared<ossia::time_node>()};

  std::shared_ptr<ossia::time_event> start_event{std::make_shared<ossia::time_event>(ossia::time_event::exec_callback{}, *start_node, ossia::expressions::make_expression_true())};
  std::shared_ptr<ossia::time_event> end_event{std::make_shared<ossia::time_event>(ossia::time_event::exec_callback{}, *end_node, ossia::expressions::make_expression_true())};

  std::shared_ptr<ossia::time_constraint> constraint{ossia::time_constraint::create([] (auto&&...) {}, *start_event, *end_event, 15000_tv, 15000_tv, 15000_tv)};
  std::shared_ptr<ossia::scenario> scenario{std::make_shared<ossia::scenario>()};

  root_scenario()
  {
    start_node->insert(start_node->get_time_events().end(), start_event);
    end_node->insert(end_node->get_time_events().end(), end_event);

    constraint->set_granularity(50_tv);
    constraint->set_drive_mode(ossia::clock::drive_mode::EXTERNAL);

    constraint->add_time_process(scenario);
    auto scen_sn = scenario->get_start_time_node();
    auto scen_se = std::make_shared<ossia::time_event>(ossia::time_event::exec_callback{}, *scen_sn, ossia::expressions::make_expression_true());
    scen_sn->insert(scen_sn->get_time_events().end(), scen_se);
  }
};

std::ostream& operator<<(std::ostream& s, ossia::time_event::status st)
{
  switch(st)
  {
    case ossia::time_event::status::NONE: s << "none"; break;
    case ossia::time_event::status::PENDING: s << "pending"; break;
    case ossia::time_event::status::HAPPENED: s << "happened"; break;
    case ossia::time_event::status::DISPOSED: s << "disposed"; break;
  }
  return s;
}

class ConstraintBenchmark : public QObject
{
  Q_OBJECT

  void add_constraint_parallel(
      ossia::scenario& s,
      ossia::time_value def = 100._tv,
      ossia::time_value min = 100._tv,
      ossia::time_value max = 100._tv)
  {
    using namespace ossia;
    auto sn = s.get_start_time_node();
    auto se = *sn->get_time_events().begin();
    auto en = std::make_shared<ossia::time_node>();
    en->set_expression(ossia::expressions::make_expression_false());
    auto ee = std::make_shared<ossia::time_event>(ossia::time_event::exec_callback{}, *en, ossia::expressions::make_expression_true());
    en->insert(en->get_time_events().end(), ee);
    auto c = ossia::time_constraint::create([] (auto&&...) {}, *se, *ee, def, min, max);
    c->set_drive_mode(ossia::clock::drive_mode::EXTERNAL);
    s.add_time_constraint(c);
  }

  void print_states(const ossia::scenario& s)
  {
    int i = 0;
    for(const auto& node : s.get_time_nodes())
    {
      std::cout << "Node " << i << "(" << node->is_evaluating() << ")";
      int j = 0;
      for(auto& ev : node->get_time_events())
      {
        std::cout << ":  Event: " << j << " => " << ev->get_status() << "\n";
        j++;
      }
      i++;
    }
  }

private Q_SLOTS:
  void test_states_rigid()
  {
      root_scenario root;
      add_constraint_parallel(*root.scenario, 2_tv, 2_tv, 2_tv);

      std::cout << "\nBefore start:\n";
      print_states(*root.scenario);

      root.constraint->start();

      std::cout << "\nAfter start:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 1:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 2:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 3:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 4:\n";
      print_states(*root.scenario);
  }


  void test_states_flexible()
  {
      root_scenario root;
      add_constraint_parallel(*root.scenario, 3_tv, 2_tv, 4_tv);

      std::cout << "\nBefore start:\n";
      print_states(*root.scenario);

      root.constraint->start();

      std::cout << "\nAfter start:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 1:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 2:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 3:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 4:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 5:\n";
      print_states(*root.scenario);
  }

  void test_states_flexible_no_min()
  {
      root_scenario root;
      add_constraint_parallel(*root.scenario, 3_tv, 0_tv, 4_tv);

      std::cout << "\nBefore start:\n";
      print_states(*root.scenario);

      root.constraint->start();

      std::cout << "\nAfter start:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 1:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 2:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 3:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 4:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 5:\n";
      print_states(*root.scenario);
  }

  void test_states_flexible_no_max()
  {
      root_scenario root;
      add_constraint_parallel(*root.scenario, 3_tv, 2_tv, ossia::Infinite);

      std::cout << "\nBefore start:\n";
      print_states(*root.scenario);

      root.constraint->start();

      std::cout << "\nAfter start:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 1:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 2:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 3:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 4:\n";
      print_states(*root.scenario);

      root.constraint->tick(1000_tv);
      std::cout << "\nAfter tick 5:\n";
      print_states(*root.scenario);
  }


  void test_basic()
  {
    return;
    root_scenario root;

    for(int i = 0; i < 10000; i++)
      add_constraint_parallel(*root.scenario);

    const int N = 1000;
    root.constraint->start();
    auto t0 = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < N; i++)
    {
      root.constraint->tick(1000_tv);
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    auto tick_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / double(N);
    qDebug() << tick_us;
  }

  void test_graph()
  {
    return;
    std::map<int, double> dur;
    for(auto k : {0, 1, 2, 5, 10, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000})
    {
      root_scenario root;

      for(int i = 0; i < k; i++)
        add_constraint_parallel(*root.scenario);

      const int N = 1000;
      root.constraint->start();
      auto t0 = std::chrono::high_resolution_clock::now();
      for(int i = 0; i < N; i++)
      {
        root.constraint->tick(1000_tv);
      }
      auto t1 = std::chrono::high_resolution_clock::now();

      auto tick_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / double(N);
      dur.insert({k, tick_us});
      qDebug() << tick_us;
    }

    for(auto e : dur)
    {
      std::cerr << e.first << ";" << e.second << "\n";
    }
  }

};


QTEST_APPLESS_MAIN(ConstraintBenchmark)

#include "ConstraintBenchmark.moc"
