#ifndef ROS2_FAULT_INJECTION_RVIZ__FAULT_INJECTION_PANEL_HPP_
#define ROS2_FAULT_INJECTION_RVIZ__FAULT_INJECTION_PANEL_HPP_

#include <memory>

#include <QLabel>
#include <QPushButton>
#include <QTableWidget>

#include <rclcpp/client.hpp>
#include <rclcpp/node.hpp>
#include <rviz_common/panel.hpp>
#include <rclcpp/executors/single_threaded_executor.hpp>
#include <QTimer>

#include "ros2_fault_injection/srv/get_fault_status.hpp"
#include "ros2_fault_injection/srv/reload_scenario.hpp"
#include "ros2_fault_injection/srv/set_fault_state.hpp"

namespace ros2_fault_injection_rviz
{

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
    void set_status_message(const QString &message);
    void handle_set_state_response(
        rclcpp::Client<ros2_fault_injection::srv::SetFaultState>::SharedFuture future);
    void handle_reload_response(
        rclcpp::Client<ros2_fault_injection::srv::ReloadScenario>::SharedFuture future);
    void set_fault_state(const std::string &fault_id, bool active);
    void populate_table(const ros2_fault_injection::srv::GetFaultStatus::Response &response);

    QPushButton *refresh_button_{nullptr};
    QPushButton *reload_button_{nullptr};
    QLabel *status_label_{nullptr};
    QTableWidget *table_{nullptr};

    rclcpp::Node::SharedPtr node_;
    rclcpp::Client<ros2_fault_injection::srv::GetFaultStatus>::SharedPtr status_client_;
    rclcpp::Client<ros2_fault_injection::srv::ReloadScenario>::SharedPtr reload_client_;
    rclcpp::Client<ros2_fault_injection::srv::SetFaultState>::SharedPtr set_state_client_;
    QTimer *spin_timer_{nullptr};
    rclcpp::executors::SingleThreadedExecutor executor_;
  };

} // namespace ros2_fault_injection_rviz

#endif // ROS2_FAULT_INJECTION_RVIZ__FAULT_INJECTION_PANEL_HPP_