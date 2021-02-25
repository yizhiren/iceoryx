// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 by Apex.AI Inc. All rights reserved.
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
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_posh/popo/publisher.hpp"
#include "iceoryx_posh/popo/sample.hpp"
#include "iceoryx_utils/cxx/unique_ptr.hpp"

#include "mocks/chunk_mock.hpp"

#include "test.hpp"

using namespace ::testing;
using ::testing::_;

// anonymous namespace to prevent linker issues or sanitizer false positives
// if a struct with the same name is used in other tests
namespace
{
struct DummyData
{
    DummyData() = default;
    uint32_t val = 42;
};
struct DummyHeader
{
    DummyHeader() = default;
    uint64_t counter = 0;
};
} // namespace

template <typename T>
class MockPublisherInterface : public iox::popo::PublisherInterface<T>
{
  public:
    void publish(iox::popo::Sample<T>&& sample) noexcept
    {
        return publishMock(std::move(sample));
    }
    MOCK_METHOD1_T(publishMock, void(iox::popo::Sample<T>&&));
};

class SampleTest : public Test
{
  public:
    SampleTest()
    {
    }

    void SetUp()
    {
    }

    void TearDown()
    {
    }

  protected:
};

TEST_F(SampleTest, PublishesSampleViaPublisherInterface)
{
    // ===== Setup ===== //
    ChunkMock<DummyData> chunk;
    iox::cxx::unique_ptr<DummyData> testSamplePtr{chunk.sample(), [](DummyData*) {}};
    MockPublisherInterface<DummyData> mockPublisherInterface{};

    auto sut = iox::popo::Sample<DummyData>(std::move(testSamplePtr), mockPublisherInterface);

    EXPECT_CALL(mockPublisherInterface, publishMock).Times(1);

    // ===== Test ===== //
    sut.publish();

    // ===== Verify ===== //
    // ===== Cleanup ===== //
}

TEST_F(SampleTest, getCutomHeaderFromNonConstTypeReturnCorrectAddress)
{
    // ===== Setup ===== //
    ChunkMock<DummyData, DummyHeader> chunk;
    iox::cxx::unique_ptr<DummyData> testSamplePtr{chunk.sample(), [](DummyData*) {}};
    MockPublisherInterface<DummyData> mockPublisherInterface{};

    auto sut = iox::popo::Sample<DummyData, const DummyHeader>(std::move(testSamplePtr), mockPublisherInterface);

    // ===== Test ===== //
    auto& customHeader = sut.getCustomHeader();

    // ===== Verify ===== //
    ASSERT_EQ(&customHeader, chunk.customHeader());

    // ===== Cleanup ===== //
}

TEST_F(SampleTest, getCutomHeaderFromConstTypeReturnCorrectAddress)
{
    // ===== Setup ===== //
    ChunkMock<DummyData, DummyHeader> chunk;
    iox::cxx::unique_ptr<const DummyData> testSamplePtr{chunk.sample(), [](const DummyData*) {}};

    auto sut = iox::popo::Sample<const DummyData, DummyHeader>(std::move(testSamplePtr));

    // ===== Test ===== //
    auto& customHeader = sut.getCustomHeader();

    // ===== Verify ===== //
    ASSERT_EQ(&customHeader, chunk.customHeader());

    // ===== Cleanup ===== //
}
