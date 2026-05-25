#include "ros2_fault_injection_rviz/fault_injection_panel.hpp"

#include <QStringList>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <pluginlib/class_list_macros.hpp>

namespace ros2_fault_injection_rviz
{

  FaultInjectionPanel::FaultInjectionPanel(QWidget *parent) : rviz_common::Panel(parent)
  {
    setup_ui();
  }

  void FaultInjectionPanel::onInitialize()
  {
    setup_ros();
  }

  void FaultInjectionPanel::setup_ui()
  {
    auto *layout = new QVBoxLayout(this);
    auto *button_layout = new QHBoxLayout();

    reload_button_ = new QPushButton("Reload Scenario", this);
    button_layout->addWidget(reload_button_);

    layout->addLayout(button_layout);

    status_label_ = new QLabel("Waiting for fault injector services", this);
    layout->addWidget(status_label_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels(QStringList{"Action", "Fault ID", "Injector", "State", "Details"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(table_);

    connect(reload_button_, &QPushButton::clicked, this, &FaultInjectionPanel::reload_scenario);
  }

  void FaultInjectionPanel::setup_ros()
  {
    node_ = std::make_shared<rclcpp::Node>("fault_injection_rviz_panel");

    executor_.add_node(node_);

    status_client_ =
        node_->create_client<ros2_fault_injection::srv::GetFaultStatus>(
            "/fault_injection/get_fault_status");
    reload_client_ =
        node_->create_client<ros2_fault_injection::srv::ReloadScenario>(
            "/fault_injection/reload_scenario");
    set_state_client_ =
        node_->create_client<ros2_fault_injection::srv::SetFaultState>(
            "/fault_injection/set_fault_state");

    spin_timer_ = new QTimer(this);
    connect(spin_timer_, &QTimer::timeout, this, [this]()
            { executor_.spin_some(); });
    spin_timer_->start(50);

    status_timer_ = new QTimer(this);
    connect(status_timer_, &QTimer::timeout, this, &FaultInjectionPanel::refresh);
    status_timer_->start(1000);

    refresh();
  }

  void FaultInjectionPanel::refresh()
  {
    if (!status_client_ || status_request_in_flight_)
    {
      return;
    }

    if (!status_client_->service_is_ready())
    {
      table_->setRowCount(0);
      set_status_message("Waiting for /fault_injection/get_fault_status");
      return;
    }

    auto request = std::make_shared<ros2_fault_injection::srv::GetFaultStatus::Request>();
    status_request_in_flight_ = true;

    status_client_->async_send_request(
        request,
        [this](rclcpp::Client<ros2_fault_injection::srv::GetFaultStatus>::SharedFuture future)
        {
          handle_status_response(future);
        });
  }

  void FaultInjectionPanel::handle_status_response(
      rclcpp::Client<ros2_fault_injection::srv::GetFaultStatus>::SharedFuture future)
  {
    status_request_in_flight_ = false;
    populate_table(*future.get());
  }

  void FaultInjectionPanel::populate_table(
      const ros2_fault_injection::srv::GetFaultStatus::Response &response)
  {
    table_->setRowCount(static_cast<int>(response.faults.size()));

    for (int row = 0; row < static_cast<int>(response.faults.size()); ++row)
    {
      const auto &fault = response.faults[row];

      const bool is_active = (fault.state == "active");
      auto *action_button = new QPushButton(is_active ? "Deactivate" : "Activate", table_);

      const std::string fault_id = fault.fault_id;
      connect(action_button, &QPushButton::clicked, this, [this, fault_id, is_active]()
              { set_fault_state(fault_id, !is_active); });

      table_->setCellWidget(row, 0, action_button);
      table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(fault.fault_id)));
      table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(fault.injector_id)));
      table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(fault.state)));
      table_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(fault.details)));
    }

    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setStretchLastSection(true);

    set_status_message(QString("Loaded %1 faults").arg(response.faults.size()));
  }

  void FaultInjectionPanel::reload_scenario()
  {
    if (!reload_client_ || !reload_client_->service_is_ready())
    {
      set_status_message("Waiting for /fault_injection/reload_scenario");
      return;
    }

    set_status_message("Reloading scenario...");

    auto request = std::make_shared<ros2_fault_injection::srv::ReloadScenario::Request>();
    reload_client_->async_send_request(
        request,
        [this](rclcpp::Client<ros2_fault_injection::srv::ReloadScenario>::SharedFuture future)
        {
          handle_reload_response(future);
        });
  }

  void FaultInjectionPanel::handle_reload_response(
      rclcpp::Client<ros2_fault_injection::srv::ReloadScenario>::SharedFuture future)
  {
    const auto response = future.get();

    set_status_message(QString::fromStdString(response->message));

    if (response->success)
    {
      refresh();
    }
  }

  void FaultInjectionPanel::set_fault_state(const std::string &fault_id, bool active)
  {
    if (!set_state_client_ || !set_state_client_->service_is_ready())
    {
      set_status_message("Waiting for /fault_injection/set_fault_state");
      return;
    }

    auto request = std::make_shared<ros2_fault_injection::srv::SetFaultState::Request>();
    request->fault_id = fault_id;
    request->active = active;

    set_state_client_->async_send_request(
        request,
        [this](rclcpp::Client<ros2_fault_injection::srv::SetFaultState>::SharedFuture future)
        {
          handle_set_state_response(future);
        });
  }

  void FaultInjectionPanel::handle_set_state_response(
      rclcpp::Client<ros2_fault_injection::srv::SetFaultState>::SharedFuture future)
  {
    const auto response = future.get();

    set_status_message(QString::fromStdString(response->message));

    if (response->success)
    {
      refresh();
    }
  }

  void FaultInjectionPanel::set_status_message(const QString &message)
  {
    if (status_label_)
    {
      status_label_->setText(message);
    }
  }

} // namespace ros2_fault_injection_rviz

PLUGINLIB_EXPORT_CLASS(ros2_fault_injection_rviz::FaultInjectionPanel, rviz_common::Panel)