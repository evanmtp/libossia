#pragma once
#include <ossia/network/base/parameter.hpp>
#include <QObject>
#include <QQmlProperty>
#include <ossia-qt/device/qml_node_base.hpp>
#include <ossia-qt/qml_context.hpp>
namespace ossia
{
namespace qt
{
class qml_node;
class qml_device;

class qml_signal : public qml_property_base
{
  Q_OBJECT

public:
  qml_signal(QQuickItem* parent = nullptr);
  ~qml_signal();

  void resetNode() override;
  void setDevice(QObject* device) override;

Q_SIGNALS:
  void triggered();
  void trigger();

private:
  void setupAddress(bool reading);
};
}
}
