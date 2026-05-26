#include "ros2_fault_injection_rviz/fault_injection_panel.hpp"

#include <QStringList>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QLineEdit>

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

    layout->addWidget(new QLabel("Recent Events", this));

    events_table_ = new QTableWidget(this);
    events_table_->setColumnCount(5);
    events_table_->setHorizontalHeaderLabels(
        QStringList{"Time", "Source", "Fault ID", "State", "Details"});
    events_table_->horizontalHeader()->setStretchLastSection(true);
    events_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    events_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(events_table_);

    config_key_dropdown_ = new QComboBox(this);
    config_value_edit_ = new QLineEdit(this);
    set_config_button_ = new QPushButton("Set Config", this);

    layout->addWidget(new QLabel("Fault Config", this));

    config_value_edit_->setPlaceholderText("Value, e.g. 1.5");
    layout->addWidget(config_key_dropdown_);
    layout->addWidget(config_value_edit_);
    layout->addWidget(set_config_button_);
    config_set_label_ = new QLabel(this);
    layout->addWidget(config_set_label_);
    set_config_button_->setEnabled(false);
    config_set_label_->setText("Select a fault to edit its config");

    connect(reload_button_, &QPushButton::clicked, this, &FaultInjectionPanel::reload_scenario);
    connect(set_config_button_, &QPushButton::clicked, this, &FaultInjectionPanel::on_set_config_clicked);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &FaultInjectionPanel::on_fault_selection_changed);
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

    event_subscription_ = node_->create_subscription<ros2_fault_injection::msg::FaultEvent>(
        "/fault_injection/events", 10,
        [this](const ros2_fault_injection::msg::FaultEvent &event)
        {
          handle_fault_event(event);
        });

    set_config_client_ =
        node_->create_client<ros2_fault_injection::srv::SetFaultConfig>(
            "/fault_injection/set_fault_config");

    get_schema_client_ =
        node_->create_client<ros2_fault_injection::srv::GetFaultSchema>(
            "/fault_injection/get_fault_schema");

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

  void FaultInjectionPanel::handle_fault_event(const ros2_fault_injection::msg::FaultEvent &event)
  {
    add_event_row(event);
  }

  void FaultInjectionPanel::add_event_row(
      const ros2_fault_injection::msg::FaultEvent &event)
  {
    if (!events_table_)
    {
      return;
    }

    events_table_->insertRow(0);

    const double seconds =
        static_cast<double>(event.stamp.sec) +
        static_cast<double>(event.stamp.nanosec) / 1e9;

    events_table_->setItem(0, 0, new QTableWidgetItem(QString::number(seconds, 'f', 3)));
    events_table_->setItem(0, 1, new QTableWidgetItem(QString::fromStdString(event.source)));
    events_table_->setItem(0, 2, new QTableWidgetItem(QString::fromStdString(event.fault_id)));
    events_table_->setItem(0, 3, new QTableWidgetItem(QString::fromStdString(event.state)));
    events_table_->setItem(0, 4, new QTableWidgetItem(QString::fromStdString(event.details)));

    while (events_table_->rowCount() > 20)
    {
      events_table_->removeRow(events_table_->rowCount() - 1);
    }

    events_table_->resizeColumnsToContents();
    events_table_->horizontalHeader()->setStretchLastSection(true);
  }

  void FaultInjectionPanel::on_fault_selection_changed()
  {
    const auto selected_items = table_->selectedItems();

    if (selected_items.empty())
    {
      selected_fault_id_.clear();
      selected_injector_id_.clear();

      config_key_dropdown_->clear();
      config_value_edit_->clear();
      set_config_button_->setEnabled(false);
      config_set_label_->setText("Select a fault to edit its config");
      return;
    }

    config_set_label_->setText("");
    const int row = selected_items.front()->row();

    const auto fault_id_item = table_->item(row, 1);
    const auto injector_id_item = table_->item(row, 2);

    if (fault_id_item == nullptr || injector_id_item == nullptr)
    {
      selected_fault_id_.clear();
      selected_injector_id_.clear();

      config_key_dropdown_->clear();
      set_config_button_->setEnabled(false);
      config_set_label_->setText("Selected row is missing fault information");
      return;
    }

    selected_fault_id_ = fault_id_item->text().toStdString();
    selected_injector_id_ = injector_id_item->text().toStdString();

    if (!get_schema_client_ || !get_schema_client_->service_is_ready())
    {
      config_key_dropdown_->clear();
      set_config_button_->setEnabled(false);
      config_set_label_->setText("Waiting for /fault_injection/get_fault_schema");
      return;
    }

    set_config_button_->setEnabled(false);
    config_key_dropdown_->clear();
    config_set_label_->setText("Loading config keys for " +
                               QString::fromStdString(selected_fault_id_));

    auto request = std::make_shared<ros2_fault_injection::srv::GetFaultSchema::Request>();
    request->fault_id = selected_fault_id_;

    const auto requested_fault_id = selected_fault_id_;
    get_schema_client_->async_send_request(
        request,
        [this, requested_fault_id](
            rclcpp::Client<ros2_fault_injection::srv::GetFaultSchema>::SharedFuture future)
        {
          if (requested_fault_id != selected_fault_id_)
          {
            return;
          }

          get_schema_response_callback(future);
        });
  }

  void FaultInjectionPanel::get_schema_response_callback(
      rclcpp::Client<ros2_fault_injection::srv::GetFaultSchema>::SharedFuture future)
  {
    const auto response = future.get();

    if (!response->success)
    {
      config_key_dropdown_->clear();
      set_config_button_->setEnabled(false);
      config_set_label_->setText("Failed to get fault schema: " +
                                 QString::fromStdString(response->message));
      return;
    }

    config_key_dropdown_->clear();
    for (const auto &key : response->keys)
    {
      config_key_dropdown_->addItem(QString::fromStdString(key));
    }

    set_config_button_->setEnabled(config_key_dropdown_->count() > 0);
    config_set_label_->setText("Selected fault: " +
                               QString::fromStdString(selected_fault_id_));
  }

  void FaultInjectionPanel::on_set_config_clicked()
  {

    if (!set_config_client_ || !set_config_client_->service_is_ready())
    {
      config_set_label_->setText("Waiting for /fault_injection/set_fault_config");
      return;
    }
    if (selected_fault_id_.empty())
    {
      config_set_label_->setText("Select a fault before editing config");
      return;
    }

    const auto key = config_key_dropdown_->currentText().trimmed().toStdString();
    const auto value = config_value_edit_->text().trimmed().toStdString();

    if (key.empty())
    {
      config_set_label_->setText("Config key cannot be empty");
      return;
    }

    if (value.empty())
    {
      config_set_label_->setText("Config value cannot be empty");
      return;
    }
    config_set_label_->setText("");
    auto request = std::make_shared<ros2_fault_injection::srv::SetFaultConfig::Request>();
    request->fault_id = selected_fault_id_;
    request->key = key;
    request->value = value;

    config_set_label_->setText("Updating " + QString::fromStdString(selected_fault_id_) + ": " + QString::fromStdString(key) + " = " + QString::fromStdString(value));

    set_config_client_->async_send_request(
        request,
        [this](rclcpp::Client<ros2_fault_injection::srv::SetFaultConfig>::SharedFuture future)
        {
          set_config_response_callback(future);
        });
  }

  void FaultInjectionPanel::set_config_response_callback(
      rclcpp::Client<ros2_fault_injection::srv::SetFaultConfig>::SharedFuture future)
  {
    const auto response = future.get();

    config_set_label_->setText(QString::fromStdString(response->message));

    if (response->success)
    {
      refresh();
    }
  }
} // namespace ros2_fault_injection_rviz

PLUGINLIB_EXPORT_CLASS(ros2_fault_injection_rviz::FaultInjectionPanel, rviz_common::Panel)