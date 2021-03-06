// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/topic_admin_connection.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/topic_mutation_builder.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(TopicAdminConnectionTest, Create) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce(
          [&](grpc::ClientContext&, google::pubsub::v1::Topic const& request) {
            EXPECT_EQ(topic.FullName(), request.name());
            return make_status_or(request);
          });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection({}, mock);
  auto const expected = TopicMutationBuilder(topic).BuildCreateMutation();
  auto response = topic_admin->CreateTopic({expected});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(TopicAdminConnectionTest, Get) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  google::pubsub::v1::Topic expected;
  expected.set_name(topic.FullName());

  EXPECT_CALL(*mock, GetTopic)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::GetTopicRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        return make_status_or(expected);
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection({}, mock);
  auto response = topic_admin->GetTopic({topic});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected));
}

TEST(TopicAdminConnectionTest, List) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();

  EXPECT_CALL(*mock, ListTopics)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::ListTopicsRequest const& request) {
        EXPECT_EQ("projects/test-project-id", request.project());
        EXPECT_TRUE(request.page_token().empty());
        google::pubsub::v1::ListTopicsResponse response;
        response.add_topics()->set_name("test-topic-01");
        response.add_topics()->set_name("test-topic-02");
        return make_status_or(response);
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection({}, mock);
  std::vector<std::string> topic_names;
  for (auto& t : topic_admin->ListTopics({"projects/test-project-id"})) {
    ASSERT_STATUS_OK(t);
    topic_names.push_back(t->name());
  }
  EXPECT_THAT(topic_names, ElementsAre("test-topic-01", "test-topic-02"));
}

/**
 * @test Verify DeleteTopic() and logging works.
 *
 * We use this test for both DeleteTopic and logging. DeleteTopic has a simple
 * return type, so it is a good candidate to do the logging test too.
 */
TEST(TopicAdminConnectionTest, DeleteWithLogging) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  auto backend =
      std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  EXPECT_CALL(*mock, DeleteTopic)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::DeleteTopicRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        return Status{};
      });

  auto topic_admin = pubsub_internal::MakeTopicAdminConnection(
      ConnectionOptions{}.enable_tracing("rpc"), mock);
  auto response = topic_admin->DeleteTopic({topic});
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(backend->log_lines, Contains(HasSubstr("DeleteTopic")));
  google::cloud::LogSink::Instance().RemoveBackend(id);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
