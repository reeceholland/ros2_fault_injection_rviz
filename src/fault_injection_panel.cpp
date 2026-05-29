#include "ros2_fault_injection_rviz/fault_injection_panel.hpp"

#include <algorithm>
#include <QStringList>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QLineEdit>
#include <initializer_list>
#include <limits>
#include <QTabWidget>
#include <QWidget>

#include <pluginlib/class_list_macros.hpp>

namespace ros2_fault_injection_rviz
{

  const FaultConfigFieldView *find_field(
      const std::vector<FaultConfigFieldView> &fields,
      const QString &key)
  {
    const auto it = std::find_if(
        fields.begin(),
        fields.end(),
        [&key](const FaultConfigFieldView &field)
        {
          return field.key == key;
        });

    if (it == fields.end())
    {
      return nullptr;
    }

    return &(*it);
  }

  QString describe_limits(const FaultConfigFieldView &field)
  {
    if (!field.has_min_value && !field.has_max_value)
    {
      return "";
    }

    QString text = "[";

    if (field.has_min_value)
    {
      text += QString::number(field.min_value);
    }
    else
    {
      text += "-";
    }

    text += ", ";

    if (field.has_max_value)
    {
      text += QString::number(field.max_value);
    }
    else
    {
      text += "-";
    }

    text += "]";

    return text;
  }

  QString describe_field(const FaultConfigFieldView &field)
  {
    QString text = field.type;

    if (field.has_min_value || field.has_max_value)
    {
      text += " [";

      if (field.has_min_value)
      {
        text += QString::number(field.min_value);
      }
      else
      {
        text += "-";
      }

      text += ", ";

      if (field.has_max_value)
      {
        text += QString::number(field.max_value);
      }
      else
      {
        text += "-";
      }

      text += "]";
    }

    if (!field.description.isEmpty())
    {
      text += " - " + field.description;
    }

    return text;
  }

  FaultInjectionPanel::FaultInjectionPanel(QWidget *parent)
      : rviz_common::Panel(parent)
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

    tabs_ = new QTabWidget(this);
    faults_tab_ = new QWidget(this);
    events_tab_ = new QWidget(this);
    config_tab_ = new QWidget(this);

    tabs_->addTab(faults_tab_, "Faults");
    tabs_->addTab(events_tab_, "Events");
    tabs_->addTab(config_tab_, "Config");

    layout->addWidget(tabs_);

    auto *faults_layout = new QVBoxLayout(faults_tab_);
    auto *faults_button_layout = new QHBoxLayout();

    reload_button_ = new QPushButton("Reload Scenario", faults_tab_);

    faults_button_layout->addWidget(reload_button_);
    faults_layout->addLayout(faults_button_layout);

    status_label_ = new QLabel("Waiting for fault injector services", faults_tab_);
    faults_layout->addWidget(status_label_);

    table_ = new QTableWidget(faults_tab_);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels(QStringList{"Action", "Fault ID", "Injector", "State",
                                                  "Details"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    faults_layout->addWidget(table_);

    auto *config_layout = new QVBoxLayout(config_tab_);

    selected_fault_label_ = new QLabel("Select a fault from the faults tab", config_tab_);
    config_layout->addWidget(selected_fault_label_);

    config_key_dropdown_ = new QComboBox(config_tab_);
    config_value_edit_ = new QLineEdit(config_tab_);
    config_bool_dropdown_ = new QComboBox(config_tab_);
    set_config_button_ = new QPushButton("Set Config", config_tab_);

    config_value_edit_->setPlaceholderText("Value, e.g. 1.5");

    config_bool_dropdown_->addItem("true");
    config_bool_dropdown_->addItem("false");
    config_bool_dropdown_->hide();

    config_int_validator_ = new QIntValidator(this);
    config_double_validator_ = new QDoubleValidator(this);
    config_double_validator_->setNotation(QDoubleValidator::StandardNotation);

    config_layout->addWidget(new QLabel("Config Key", config_tab_));
    config_layout->addWidget(config_key_dropdown_);
    config_layout->addWidget(new QLabel("Value", config_tab_));
    config_layout->addWidget(config_value_edit_);
    config_layout->addWidget(config_bool_dropdown_);
    config_layout->addWidget(set_config_button_);

    config_set_label_ = new QLabel("Select a fault to edit its config", config_tab_);
    config_set_label_->setWordWrap(true);
    config_layout->addWidget(config_set_label_);

    set_config_button_->setEnabled(false);

    config_table_ = new QTableWidget(config_tab_);
    config_table_->setColumnCount(5);
    config_table_->setHorizontalHeaderLabels(QStringList{"Key", "Type", "Current Value", "Limits",
                                                         "Description"});
    config_table_->horizontalHeader()->setStretchLastSection(true);
    config_layout->addWidget(config_table_);

    auto *events_layout = new QVBoxLayout(events_tab_);

    events_table_ = new QTableWidget(events_tab_);
    events_table_->setColumnCount(5);
    events_table_->setHorizontalHeaderLabels(
        QStringList{"Time", "Source", "Fault ID", "State", "Details"});
    events_table_->horizontalHeader()->setStretchLastSection(true);
    events_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    events_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    events_layout->addWidget(events_table_);

    connect(reload_button_, &QPushButton::clicked, this, &FaultInjectionPanel::reload_scenario);
    connect(set_config_button_, &QPushButton::clicked, this,
            &FaultInjectionPanel::on_set_config_clicked);
    connect(table_, &QTableWidget::itemSelectionChanged, this,
            &FaultInjectionPanel::on_fault_selection_changed);
    connect(config_key_dropdown_, &QComboBox::currentTextChanged, this,
            &FaultInjectionPanel::on_config_key_changed);
    connect(config_table_, &QTableWidget::itemSelectionChanged, this,
            &FaultInjectionPanel::on_config_table_selection_changed);
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

    get_config_client_ =
        node_->create_client<ros2_fault_injection::srv::GetFaultConfig>(
            "/fault_injection/get_fault_config");

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
      selected_fault_.clear();
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
      selected_fault_.clear();

      config_key_dropdown_->clear();
      config_value_edit_->clear();
      config_value_edit_->show();
      config_bool_dropdown_->hide();
      set_config_button_->setEnabled(false);
      config_set_label_->setText("Select a fault from the faults tab");
      return;
    }

    config_set_label_->setText("");
    const int row = selected_items.front()->row();

    const auto fault_id_item = table_->item(row, 1);
    const auto injector_id_item = table_->item(row, 2);

    if (fault_id_item == nullptr || injector_id_item == nullptr)
    {
      selected_fault_.clear();

      config_key_dropdown_->clear();
      config_value_edit_->clear();
      config_value_edit_->show();
      config_bool_dropdown_->hide();
      set_config_button_->setEnabled(false);
      config_set_label_->setText("Selected row is missing fault information");
      return;
    }

    selected_fault_.clear();
    selected_fault_.fault_id = fault_id_item->text().toStdString();
    selected_fault_.injector_id = injector_id_item->text().toStdString();

    selected_fault_label_->setText(
        QString("Selected Fault: %1 (Injector: %2)")
            .arg(QString::fromStdString(selected_fault_.fault_id))
            .arg(QString::fromStdString(selected_fault_.injector_id)));

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
                               QString::fromStdString(selected_fault_.fault_id));

    auto request = std::make_shared<ros2_fault_injection::srv::GetFaultSchema::Request>();
    request->fault_id = selected_fault_.fault_id;

    const auto requested_fault_id = selected_fault_.fault_id;
    get_schema_client_->async_send_request(
        request,
        [this, requested_fault_id](
            rclcpp::Client<ros2_fault_injection::srv::GetFaultSchema>::SharedFuture future)
        {
          if (requested_fault_id != selected_fault_.fault_id)
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
      selected_fault_schema_.clear();
      config_key_dropdown_->clear();
      set_config_button_->setEnabled(false);
      config_set_label_->setText(
          "Failed to get fault schema: " + QString::fromStdString(response->message));
      return;
    }

    selected_fault_.injector_type = response->injector_type;
    selected_fault_.config_keys = response->keys;
    selected_fault_schema_.clear();

    selected_fault_label_->setText(
        QString("Selected Fault: %1 (Injector: %2, Type: %3)")
            .arg(QString::fromStdString(selected_fault_.fault_id))
            .arg(QString::fromStdString(selected_fault_.injector_id))
            .arg(QString::fromStdString(selected_fault_.injector_type)));

    const auto field_count = std::min({response->keys.size(),
                                       response->types.size(),
                                       response->descriptions.size(),
                                       response->default_values.size(),
                                       response->has_min_values.size(),
                                       response->min_values.size(),
                                       response->has_max_values.size(),
                                       response->max_values.size()});

    selected_fault_schema_.reserve(field_count);

    for (size_t i = 0; i < field_count; ++i)
    {
      FaultConfigFieldView field;
      field.key = QString::fromStdString(response->keys[i]);
      field.type = QString::fromStdString(response->types[i]);
      field.description = QString::fromStdString(response->descriptions[i]);
      field.default_value = QString::fromStdString(response->default_values[i]);
      field.has_min_value = response->has_min_values[i];
      field.min_value = response->min_values[i];
      field.has_max_value = response->has_max_values[i];
      field.max_value = response->max_values[i];

      selected_fault_schema_.push_back(field);
    }

    config_key_dropdown_->clear();

    for (const auto &field : selected_fault_schema_)
    {
      config_key_dropdown_->addItem(field.key);
    }

    if (!get_config_client_ || !get_config_client_->service_is_ready())
    {
      config_value_edit_->clear();
      set_config_button_->setEnabled(false);
      config_set_label_->setText("Waiting for /fault_injection/get_fault_config");
      return;
    }

    set_config_button_->setEnabled(config_key_dropdown_->count() > 0);

    auto request = std::make_shared<ros2_fault_injection::srv::GetFaultConfig::Request>();
    request->fault_id = selected_fault_.fault_id;

    const auto requested_fault_id = selected_fault_.fault_id;
    get_config_client_->async_send_request(
        request,
        [this, requested_fault_id](
            rclcpp::Client<ros2_fault_injection::srv::GetFaultConfig>::SharedFuture future)
        {
          if (requested_fault_id != selected_fault_.fault_id)
          {
            return;
          }

          get_config_response_callback(future);
        });
  }

  void FaultInjectionPanel::get_config_response_callback(
      rclcpp::Client<ros2_fault_injection::srv::GetFaultConfig>::SharedFuture future)
  {
    const auto response = future.get();

    if (!response->success)
    {
      config_value_edit_->clear();
      config_set_label_->setText("Failed to get fault config: " +
                                 QString::fromStdString(response->message));
      return;
    }
    config_value_edit_->clear();
    selected_fault_.config_values.clear();
    const auto pair_count = std::min(response->keys.size(), response->values.size());
    for (size_t i = 0; i < pair_count; ++i)
    {
      selected_fault_.config_values[response->keys[i]] = response->values[i];
    }

    on_config_key_changed(config_key_dropdown_->currentText());
  }

  void FaultInjectionPanel::on_set_config_clicked()
  {
    if (!set_config_client_ || !set_config_client_->service_is_ready())
    {
      config_set_label_->setText("Waiting for /fault_injection/set_fault_config");
      return;
    }

    if (selected_fault_.fault_id.empty())
    {
      config_set_label_->setText("Select a fault before editing config");
      return;
    }

    const auto key_qt = config_key_dropdown_->currentText().trimmed();
    const auto *field = find_field(selected_fault_schema_, key_qt);

    const auto key = key_qt.toStdString();

    std::string value;
    if (field != nullptr && field->type == "bool")
    {
      value = config_bool_dropdown_->currentText().trimmed().toStdString();
    }
    else
    {
      value = config_value_edit_->text().trimmed().toStdString();
    }

    if (key.empty())
    {
      config_set_label_->setText("Config key cannot be empty");
      return;
    }

    if (field != nullptr && (field->type == "int" || field->type == "double") && !config_value_edit_->hasAcceptableInput())
    {
      config_set_label_->setText("Invalid value for " + key_qt + ": " + config_value_edit_->text());
      return;
    }

    if (value.empty())
    {
      if (field != nullptr && field->type == "string")
      {
        value = "";
      }
      else
      {
        config_set_label_->setText("Config value cannot be empty");
        return;
      }
    }

    config_set_label_->setText("");

    auto request = std::make_shared<ros2_fault_injection::srv::SetFaultConfig::Request>();
    request->fault_id = selected_fault_.fault_id;
    request->key = key;
    request->value = value;

    config_set_label_->setText(
        "Updating " + QString::fromStdString(selected_fault_.fault_id) + ": " +
        QString::fromStdString(key) + " = " + QString::fromStdString(value));

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

      if (!selected_fault_.fault_id.empty() && get_config_client_ && get_config_client_->service_is_ready())
      {
        auto request = std::make_shared<ros2_fault_injection::srv::GetFaultConfig::Request>();
        request->fault_id = selected_fault_.fault_id;

        const auto requested_fault_id = selected_fault_.fault_id;
        get_config_client_->async_send_request(
            request,
            [this, requested_fault_id](
                rclcpp::Client<ros2_fault_injection::srv::GetFaultConfig>::SharedFuture future)
            {
              if (requested_fault_id != selected_fault_.fault_id)
              {
                return;
              }

              get_config_response_callback(future);
            });
      }
    }
  }

  void FaultInjectionPanel::on_config_key_changed(const QString &key)
  {
    const auto key_str = key.trimmed().toStdString();

    config_value_edit_->setValidator(nullptr);
    config_value_edit_->show();
    config_bool_dropdown_->hide();

    if (key_str.empty())
    {
      config_value_edit_->clear();
      return;
    }

    const auto *field = find_field(selected_fault_schema_, key);

    const auto value_it = selected_fault_.config_values.find(key_str);
    const QString current_value =
        value_it != selected_fault_.config_values.end()
            ? QString::fromStdString(value_it->second)
            : "";

    update_config_table();

    if (field != nullptr && field->type == "bool")
    {
      config_bool_dropdown_->show();
      config_value_edit_->hide();

      const int index = config_bool_dropdown_->findText(current_value);
      config_bool_dropdown_->setCurrentIndex(index >= 0 ? index : 0);
    }
    else
    {
      config_value_edit_->setText(current_value);

      if (field != nullptr && field->type == "int")
      {
        const int min_value = field->has_min_value ? static_cast<int>(field->min_value) : std::numeric_limits<int>::min();
        const int max_value = field->has_max_value ? static_cast<int>(field->max_value) : std::numeric_limits<int>::max();
        config_int_validator_->setRange(min_value, max_value);
        config_value_edit_->setValidator(config_int_validator_);
      }
      else if (field != nullptr && field->type == "double")
      {
        const double min_value = field->has_min_value ? field->min_value : -std::numeric_limits<double>::max();
        const double max_value = field->has_max_value ? field->max_value : std::numeric_limits<double>::max();
        config_double_validator_->setRange(min_value, max_value);
        config_value_edit_->setValidator(config_double_validator_);
      }
    }

    if (field != nullptr)
    {
      config_set_label_->setText(describe_field(*field));
    }
  }

  void FaultInjectionPanel::update_config_table()
  {
    config_table_->setRowCount(0);
    for (const auto &field : selected_fault_schema_)
    {
      const auto value_it = selected_fault_.config_values.find(field.key.toStdString());
      const QString current_value =
          value_it != selected_fault_.config_values.end()
              ? QString::fromStdString(value_it->second)
              : "";

      const int row = config_table_->rowCount();
      config_table_->insertRow(row);
      config_table_->setItem(row, 0, new QTableWidgetItem(field.key));
      config_table_->setItem(row, 1, new QTableWidgetItem(field.type));
      config_table_->setItem(row, 2, new QTableWidgetItem(current_value));
      config_table_->setItem(row, 3, new QTableWidgetItem(describe_limits(field)));
      config_table_->setItem(row, 4, new QTableWidgetItem(field.description));
    }
  }

  void FaultInjectionPanel::on_config_table_selection_changed()
  {
    const auto selected_items = config_table_->selectedItems();

    if (selected_items.empty())
    {
      return;
    }

    const int row = selected_items.front()->row();
    const auto key_item = config_table_->item(row, 0);

    if (key_item == nullptr)
    {
      return;
    }

    const QString key = key_item->text();
    config_key_dropdown_->setCurrentText(key);
  }
} // namespace ros2_fault_injection_rviz

PLUGINLIB_EXPORT_CLASS(ros2_fault_injection_rviz::FaultInjectionPanel, rviz_common::Panel)
