#ifndef ROS2_FAULT_INJECTION_RVIZ__FAULT_INJECTION_PANEL_HPP_
#define ROS2_FAULT_INJECTION_RVIZ__FAULT_INJECTION_PANEL_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QComboBox>

#include <rclcpp/client.hpp>
#include <rclcpp/executors/single_threaded_executor.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/subscription.hpp>
#include <rviz_common/panel.hpp>

#include "ros2_fault_injection/msg/fault_event.hpp"
#include "ros2_fault_injection/srv/get_fault_status.hpp"
#include "ros2_fault_injection/srv/reload_scenario.hpp"
#include "ros2_fault_injection/srv/set_fault_state.hpp"
#include "ros2_fault_injection/srv/set_fault_config.hpp"
#include "ros2_fault_injection/srv/get_fault_schema.hpp"
#include "ros2_fault_injection/srv/get_fault_config.hpp"

namespace ros2_fault_injection_rviz
{

struct SelectedFault
{
  std::string fault_id;
  std::string injector_id;
  std::string injector_type;
  std::vector<std::string> config_keys;
  std::unordered_map<std::string, std::string> config_values;

  bool valid() const
  {
    return !fault_id.empty();
  }

  void clear()
  {
    fault_id.clear();
    injector_id.clear();
    injector_type.clear();
    config_keys.clear();
    config_values.clear();
  }
};

class FaultInjectionPanel : public rviz_common::Panel
{
  Q_OBJECT

public:
  explicit FaultInjectionPanel(QWidget *parent = nullptr);

  void onInitialize() override;

private Q_SLOTS:
  void refresh();
  void reload_scenario();

private:
  void setup_ui();
  void setup_ros();
  void handle_status_response(
    rclcpp::Client<ros2_fault_injection::srv::GetFaultStatus>::SharedFuture future);
  void set_status_message(const QString & message);
  void handle_set_state_response(
    rclcpp::Client<ros2_fault_injection::srv::SetFaultState>::SharedFuture future);
  void handle_reload_response(
    rclcpp::Client<ros2_fault_injection::srv::ReloadScenario>::SharedFuture future);
  void set_fault_state(const std::string & fault_id, bool active);
  void populate_table(const ros2_fault_injection::srv::GetFaultStatus::Response & response);
  void handle_fault_event(const ros2_fault_injection::msg::FaultEvent & event);
  void add_event_row(const ros2_fault_injection::msg::FaultEvent & event);
  void on_fault_selection_changed();
  void on_set_config_clicked();
  void on_config_key_changed(const QString & key);
  void set_config_response_callback(
    rclcpp::Client<ros2_fault_injection::srv::SetFaultConfig>::SharedFuture future);
  void get_schema_response_callback(
    rclcpp::Client<ros2_fault_injection::srv::GetFaultSchema>::SharedFuture future);
  void get_config_response_callback(
    rclcpp::Client<ros2_fault_injection::srv::GetFaultConfig>::SharedFuture future);
  QPushButton *reload_button_{nullptr};
  QLabel *status_label_{nullptr};
  QTableWidget *table_{nullptr};
  QTableWidget *events_table_{nullptr};

  rclcpp::Node::SharedPtr node_;
  rclcpp::Client<ros2_fault_injection::srv::GetFaultStatus>::SharedPtr status_client_;
  rclcpp::Client<ros2_fault_injection::srv::ReloadScenario>::SharedPtr reload_client_;
  rclcpp::Client<ros2_fault_injection::srv::SetFaultState>::SharedPtr set_state_client_;
  rclcpp::Subscription<ros2_fault_injection::msg::FaultEvent>::SharedPtr event_subscription_;
  QTimer *spin_timer_{nullptr};
  QTimer *status_timer_{nullptr};
  bool status_request_in_flight_{false};
  rclcpp::executors::SingleThreadedExecutor executor_;
  QLabel *config_set_label_{nullptr};
  QLineEdit *config_value_edit_{nullptr};
  QPushButton *set_config_button_{nullptr};
  rclcpp::Client<ros2_fault_injection::srv::SetFaultConfig>::SharedPtr set_config_client_;
  rclcpp::Client<ros2_fault_injection::srv::GetFaultSchema>::SharedPtr get_schema_client_;
  rclcpp::Client<ros2_fault_injection::srv::GetFaultConfig>::SharedPtr get_config_client_;
  QComboBox *config_key_dropdown_{nullptr};
  SelectedFault selected_fault_;
};

} // namespace ros2_fault_injection_rviz

#endif // ROS2_FAULT_INJECTION_RVIZ__FAULT_INJECTION_PANEL_HPP_
