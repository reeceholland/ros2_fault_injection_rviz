#include <chrono>
#include <memory>

#include <QApplication>
#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include "ros2_fault_injection_rviz/fault_injection_panel.hpp"

namespace ros2_fault_injection_rviz
{
  class FaultInjectionPanelTest : public ::testing::Test
  {
  protected:
    using Clock = std::chrono::steady_clock;
    using Service = ros2_fault_injection::srv::GetFaultStatus;

    static void SetUpTestSuite()
    {
      qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
      static int argc = 1;
      static char name[] = "test_status_timeout";
      static char *argv[] = {name, nullptr};
      app_ = std::make_unique<QApplication>(argc, argv);

      rclcpp::init(0, nullptr);
    }

    static void TearDownTestSuite()
    {
      rclcpp::shutdown();
      app_.reset();
    }

    void SetUp() override
    {
      panel_ = std::make_unique<FaultInjectionPanel>();

      // Constructor creates the UI only; ROS timers are not started in this fixture.

      node_ = std::make_shared<rclcpp::Node>("status_timeout_test");

      service_ = node_->create_service<Service>(
          "/status_timeout_test/status",
          [](const std::shared_ptr<Service::Request>,
             const std::shared_ptr<Service::Response>) {});

      panel_->status_client_ = node_->create_client<Service>(
          "/status_timeout_test/status");

      ASSERT_TRUE(panel_->status_client_->wait_for_service(std::chrono::seconds(5)));

      panel_->pending_status_request_.reset();
    }

    void TearDown() override
    {
      panel_.reset();
      node_.reset();
      service_.reset();
    }

    int64_t begin_request(Clock::time_point deadline)
    {
      auto pending = panel_->status_client_->async_send_request(
          std::make_shared<Service::Request>());

      panel_->pending_status_request_ =
          PendingRequest{pending.request_id, deadline};
      return pending.request_id;
    }

    void check_timeout(Clock::time_point now)
    {
      panel_->check_status_timeout(now);
    }

    bool has_pending_request() const
    {
      return panel_->pending_status_request_.has_value();
    }

    void refresh()
    {
      panel_->refresh();
    }

    Clock::time_point deadline() const
    {
      return panel_->pending_status_request_->deadline;
    }

    int64_t request_id() const
    {
      return panel_->pending_status_request_->request_id;
    }

    bool remove_request(int64_t id)
    {
      return panel_->status_client_->remove_pending_request(id);
    }

    QString status() const
    {
      return panel_->status_label_->text();
    }

    bool receive_response()
    {
      const auto limit = Clock::now() + std::chrono::seconds(3);
      while (has_pending_request() && Clock::now() < limit) {
        rclcpp::spin_some(node_);
      }
      return !has_pending_request();
    }

    void process_late_response()
    {
      // Service the expired request, then give DDS time to deliver its response.
      const auto limit = Clock::now() + std::chrono::milliseconds(200);
      while (Clock::now() < limit) {
        rclcpp::spin_some(node_);
      }
    }

    inline static std::unique_ptr<QApplication> app_;
    std::unique_ptr<FaultInjectionPanel> panel_;
    rclcpp::Node::SharedPtr node_;
    rclcpp::Service<Service>::SharedPtr service_;
  };

  TEST_F(FaultInjectionPanelTest, DoNothingWithoutPendingRequest)
  {
    ASSERT_FALSE(has_pending_request());

    check_timeout(Clock::now() + std::chrono::seconds(10));

    EXPECT_FALSE(has_pending_request());
  }

  TEST_F(FaultInjectionPanelTest, KeepsRequestBeforeDeadline)
  {
    const auto start = Clock::now();
    begin_request(start + std::chrono::seconds(3));
    check_timeout(start + std::chrono::seconds(2));
    EXPECT_TRUE(has_pending_request());
  }

  TEST_F(FaultInjectionPanelTest, ClearsRequestAtDeadline)
  {
    const auto start = Clock::now();
    begin_request(start + std::chrono::seconds(3));
    check_timeout(start + std::chrono::seconds(3));
    EXPECT_FALSE(has_pending_request());
  }

  TEST_F(FaultInjectionPanelTest, RemovesTimedOutRequestFromClient)
  {
    const auto start = Clock::now();
    const auto id = begin_request(start + std::chrono::seconds(3));
    check_timeout(start + std::chrono::seconds(4));
    EXPECT_FALSE(has_pending_request());
    // False means the timeout handler already removed it from the ROS client.
    EXPECT_FALSE(remove_request(id));
  }

  TEST_F(FaultInjectionPanelTest, ShowsWarningOnTimeout)
  {
    refresh();
    ASSERT_TRUE(has_pending_request());
    check_timeout(deadline());
    EXPECT_TRUE(status().contains("status request timed out"));
  }

  TEST_F(FaultInjectionPanelTest, ResponseBeforeDeadlinePreventsTimeout)
  {
    refresh();
    ASSERT_TRUE(has_pending_request());
    const auto expires = deadline();
    ASSERT_TRUE(receive_response());
    const auto after_response = status();
    check_timeout(expires + std::chrono::seconds(1));
    EXPECT_FALSE(has_pending_request());
    EXPECT_EQ(status(), after_response);
    EXPECT_FALSE(status().contains("timed out"));
  }

  TEST_F(FaultInjectionPanelTest, RetriesAfterTimeout)
  {
    refresh();
    ASSERT_TRUE(has_pending_request());
    const auto old_id = request_id();
    check_timeout(deadline());
    ASSERT_FALSE(has_pending_request());
    refresh();
    ASSERT_TRUE(has_pending_request());
    EXPECT_NE(request_id(), old_id);
    EXPECT_TRUE(receive_response());
  }

  TEST_F(FaultInjectionPanelTest, IgnoresResponseForRemovedRequest)
  {
    refresh();
    ASSERT_TRUE(has_pending_request());
    check_timeout(deadline());
    const auto timeout_message = status();
    process_late_response();
    EXPECT_FALSE(has_pending_request());
    EXPECT_EQ(status(), timeout_message);
  }
} // namespace ros2_fault_injection_rviz
