// Copyright (c) 2020 - 2022 by Apex.AI Inc. All rights reserved.
// Copyright (c) 2020 - 2021 by Robert Bosch GmbH. All rights reserved.
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

#include "iceoryx_hoofs/cxx/helplets.hpp"
#include "iceoryx_hoofs/testing/timing_test.hpp"
#include "iceoryx_posh/popo/untyped_publisher.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iceoryx_posh/runtime/service_discovery.hpp"
#include "iceoryx_posh/testing/roudi_gtest.hpp"
#include "mocks/posh_runtime_mock.hpp"
#include "test.hpp"


#include <type_traits>

namespace
{
using namespace ::testing;
using namespace iox::runtime;
using namespace iox::cxx;
using iox::capro::IdString_t;
using iox::capro::ServiceDescription;
using iox::roudi::RouDiEnvironment;
using iox::runtime::ServiceContainer;

class ServiceDiscovery_test : public RouDi_GTest
{
  public:
    void SetUp() override
    {
        searchResultOfFindServiceWithFindHandler.clear();
    }

    void TearDown() override
    {
    }

    iox::runtime::PoshRuntime* runtime{&iox::runtime::PoshRuntime::initRuntime("Runtime")};
    ServiceDiscovery sut;

    static ServiceContainer searchResultOfFindServiceWithFindHandler;
    static void findHandler(const ServiceContainer& s)
    {
        searchResultOfFindServiceWithFindHandler = s;
    };
};

ServiceContainer ServiceDiscovery_test::searchResultOfFindServiceWithFindHandler;

void compareServiceContainers(const ServiceContainer& lhs, const ServiceContainer& rhs)
{
    const auto size = lhs.size();
    ASSERT_THAT(size, Eq(rhs.size()));
    for (auto i = 0U; i < size; i++)
    {
        EXPECT_THAT(lhs[i], Eq(rhs[i]));
    }
}

TIMING_TEST_F(ServiceDiscovery_test, GetServiceRegistryChangeCounterOfferStopOfferService, Repeat(5), [&] {
    auto serviceCounter = sut.getServiceRegistryChangeCounter();
    auto initialCout = serviceCounter->load();

    iox::popo::UntypedPublisher pub({"service", "instance", "event"});

    TIMING_TEST_EXPECT_TRUE(initialCout + 1 == serviceCounter->load());

    pub.stopOffer();
    this->InterOpWait();

    TIMING_TEST_EXPECT_TRUE(initialCout + 2 == serviceCounter->load());
});

TEST_F(ServiceDiscovery_test, FindServiceWithWildcardsReturnsOnlyIntrospectionServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "d944f32c-edef-44f5-a6eb-c19ee73c98eb");
    auto serviceContainer = sut.findService(iox::capro::Wildcard, iox::capro::Wildcard, iox::capro::Wildcard);
    ASSERT_FALSE(serviceContainer.has_error());

    auto searchResult = serviceContainer.value();

    for (auto& service : searchResult)
    {
        EXPECT_THAT(service.getServiceIDString().c_str(), StrEq("Introspection"));
    }

    sut.findService(iox::capro::Wildcard, iox::capro::Wildcard, iox::capro::Wildcard, findHandler);
    compareServiceContainers(searchResult, searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, FindServiceReturnsOfferedService)
{
    ::testing::Test::RecordProperty("TEST_ID", "30f0e255-3584-4ab2-b7a6-85c16026852d");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);

    auto serviceContainer = sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                                            SERVICE_DESCRIPTION.getInstanceIDString(),
                                            SERVICE_DESCRIPTION.getEventIDString());

    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION));

    sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                    SERVICE_DESCRIPTION.getInstanceIDString(),
                    SERVICE_DESCRIPTION.getEventIDString(),
                    findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, ReofferedServiceWithValidServiceDescriptionCanBeFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "b67b4990-e2fd-4efa-ab5d-e53c4ee55972");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);

    auto serviceContainer = sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                                            SERVICE_DESCRIPTION.getInstanceIDString(),
                                            SERVICE_DESCRIPTION.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION));
    sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                    SERVICE_DESCRIPTION.getInstanceIDString(),
                    SERVICE_DESCRIPTION.getEventIDString(),
                    findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    publisher.stopOffer();
    this->InterOpWait();
    serviceContainer = sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                                       SERVICE_DESCRIPTION.getInstanceIDString(),
                                       SERVICE_DESCRIPTION.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_TRUE(serviceContainer.value().empty());
    sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                    SERVICE_DESCRIPTION.getInstanceIDString(),
                    SERVICE_DESCRIPTION.getEventIDString(),
                    findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    publisher.offer();
    this->InterOpWait();

    serviceContainer = sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                                       SERVICE_DESCRIPTION.getInstanceIDString(),
                                       SERVICE_DESCRIPTION.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION));
    sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                    SERVICE_DESCRIPTION.getInstanceIDString(),
                    SERVICE_DESCRIPTION.getEventIDString(),
                    findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, OfferExsistingServiceMultipleTimesIsRedundant)
{
    ::testing::Test::RecordProperty("TEST_ID", "ae0790ed-4e1b-4f12-94b3-c9e56433c935");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);
    publisher.offer();
    this->InterOpWait();

    auto serviceContainer = sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                                            SERVICE_DESCRIPTION.getInstanceIDString(),
                                            SERVICE_DESCRIPTION.getEventIDString());

    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION));

    sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                    SERVICE_DESCRIPTION.getInstanceIDString(),
                    SERVICE_DESCRIPTION.getEventIDString(),
                    findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, FindSameServiceMultipleTimesReturnsSingleInstance)
{
    ::testing::Test::RecordProperty("TEST_ID", "21948bcf-fe7e-44b4-b93b-f46303e3e050");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);

    auto serviceContainer = sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                                            SERVICE_DESCRIPTION.getInstanceIDString(),
                                            SERVICE_DESCRIPTION.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION));
    sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                    SERVICE_DESCRIPTION.getInstanceIDString(),
                    SERVICE_DESCRIPTION.getEventIDString(),
                    findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                                       SERVICE_DESCRIPTION.getInstanceIDString(),
                                       SERVICE_DESCRIPTION.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION));
    sut.findService(SERVICE_DESCRIPTION.getServiceIDString(),
                    SERVICE_DESCRIPTION.getInstanceIDString(),
                    SERVICE_DESCRIPTION.getEventIDString(),
                    findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, OfferDifferentServicesWithSameInstanceAndEvent)
{
    ::testing::Test::RecordProperty("TEST_ID", "25bf794d-450e-47ce-a920-ab2ea479af39");

    const IdString_t INSTANCE = "instance";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3("service3", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd3(SERVICE_DESCRIPTION3);

    auto serviceContainer = sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION1));
    sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(), INSTANCE, EVENT, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE_DESCRIPTION2.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION2));
    sut.findService(SERVICE_DESCRIPTION2.getServiceIDString(), INSTANCE, EVENT, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE_DESCRIPTION3.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION3));
    sut.findService(SERVICE_DESCRIPTION3.getServiceIDString(), INSTANCE, EVENT, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, FindServiceDoesNotReturnServiceWhenStringsDoNotMatch)
{
    ::testing::Test::RecordProperty("TEST_ID", "1984e907-e990-48b2-8cbd-eab3f67cd162");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", "instance1", "event1");
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", "instance2", "event2");
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);

    auto serviceContainer = sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                                            SERVICE_DESCRIPTION1.getInstanceIDString(),
                                            SERVICE_DESCRIPTION2.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                    SERVICE_DESCRIPTION1.getInstanceIDString(),
                    SERVICE_DESCRIPTION2.getEventIDString(),
                    findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                                       SERVICE_DESCRIPTION2.getInstanceIDString(),
                                       SERVICE_DESCRIPTION1.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                    SERVICE_DESCRIPTION2.getInstanceIDString(),
                    SERVICE_DESCRIPTION1.getEventIDString(),
                    findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                                       SERVICE_DESCRIPTION2.getInstanceIDString(),
                                       SERVICE_DESCRIPTION2.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                    SERVICE_DESCRIPTION2.getInstanceIDString(),
                    SERVICE_DESCRIPTION2.getEventIDString(),
                    findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());
}

TEST_F(ServiceDiscovery_test, FindServiceWithInstanceAndEventWildcardReturnsAllMatchingServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "6e0b1a12-6995-45f4-8fd8-59acbca9bfa8");

    const IdString_t SERVICE = "service";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, "instance1", "event1");
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2(SERVICE, "instance2", "event2");
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, "instance3", "event3");
    iox::popo::UntypedPublisher publisher_sd3(SERVICE_DESCRIPTION3);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION2);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    auto serviceContainer = sut.findService(SERVICE, iox::capro::Wildcard, iox::capro::Wildcard);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(3U));
    EXPECT_TRUE(serviceContainer.value() == serviceContainerExp);

    sut.findService(SERVICE, iox::capro::Wildcard, iox::capro::Wildcard, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler == serviceContainerExp);
}

TEST_F(ServiceDiscovery_test, FindServiceWithServiceWildcardReturnsCorrectServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "f4731a52-39d8-4d49-b247-008a2e9181f9");
    const IdString_t INSTANCE = "instance";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", "another_instance", EVENT);
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3("service3", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd3(SERVICE_DESCRIPTION3);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION4("service4", INSTANCE, "another_event");
    iox::popo::UntypedPublisher publisher_sd4(SERVICE_DESCRIPTION4);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    auto serviceContainer = sut.findService(iox::capro::Wildcard, INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(2U));
    EXPECT_TRUE(serviceContainer.value() == serviceContainerExp);

    sut.findService(iox::capro::Wildcard, INSTANCE, EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler == serviceContainerExp);
}

TEST_F(ServiceDiscovery_test, FindServiceWithEventWildcardReturnsCorrectServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "e78b35f4-b3c3-4c39-b10a-67712c72e28a");
    const IdString_t SERVICE = "service";
    const IdString_t INSTANCE = "instance";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, INSTANCE, "event1");
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("another_service", INSTANCE, "event2");
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, INSTANCE, "event3");
    iox::popo::UntypedPublisher publisher_sd3(SERVICE_DESCRIPTION3);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION4(SERVICE, "another_instance", "event4");
    iox::popo::UntypedPublisher publisher_sd4(SERVICE_DESCRIPTION4);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    auto serviceContainer = sut.findService(SERVICE, INSTANCE, iox::capro::Wildcard);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(2U));
    EXPECT_TRUE(serviceContainer.value() == serviceContainerExp);

    sut.findService(SERVICE, INSTANCE, iox::capro::Wildcard, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler == serviceContainerExp);
}

TEST_F(ServiceDiscovery_test, FindServiceWithInstanceWildcardReturnsCorrectServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "2ec4b422-3ded-4af3-9e72-3b870c55031c");
    const IdString_t SERVICE = "service";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, "instance1", EVENT);
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("another_service", "instance2", EVENT);
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, "instance3", EVENT);
    iox::popo::UntypedPublisher publisher_sd3(SERVICE_DESCRIPTION3);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION4(SERVICE, "instance4", "another_event");
    iox::popo::UntypedPublisher publisher_sd4(SERVICE_DESCRIPTION4);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    auto serviceContainer = sut.findService(SERVICE, iox::capro::Wildcard, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(2U));
    EXPECT_TRUE(serviceContainer.value() == serviceContainerExp);

    sut.findService(SERVICE, iox::capro::Wildcard, EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler == serviceContainerExp);
}

TEST_F(ServiceDiscovery_test, OfferSingleServiceMultiInstance)
{
    ::testing::Test::RecordProperty("TEST_ID", "538bec69-ea02-400e-8643-c833d6e84972");

    const IdString_t SERVICE = "service";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, "instance1", "event1");
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2(SERVICE, "instance2", "event2");
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, "instance3", "event3");
    iox::popo::UntypedPublisher publisher_sd3(SERVICE_DESCRIPTION3);

    auto serviceContainer =
        sut.findService(SERVICE, SERVICE_DESCRIPTION1.getInstanceIDString(), SERVICE_DESCRIPTION1.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION1));
    sut.findService(
        SERVICE, SERVICE_DESCRIPTION1.getInstanceIDString(), SERVICE_DESCRIPTION1.getEventIDString(), findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer =
        sut.findService(SERVICE, SERVICE_DESCRIPTION2.getInstanceIDString(), SERVICE_DESCRIPTION2.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION2));
    sut.findService(
        SERVICE, SERVICE_DESCRIPTION2.getInstanceIDString(), SERVICE_DESCRIPTION2.getEventIDString(), findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer =
        sut.findService(SERVICE, SERVICE_DESCRIPTION3.getInstanceIDString(), SERVICE_DESCRIPTION3.getEventIDString());
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION3));
    sut.findService(
        SERVICE, SERVICE_DESCRIPTION3.getInstanceIDString(), SERVICE_DESCRIPTION3.getEventIDString(), findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, FindServiceReturnsCorrectServiceInstanceCombinations)
{
    ::testing::Test::RecordProperty("TEST_ID", "360839a7-9309-4e7e-8e89-892097a87f7a");

    const IdString_t SERVICE1 = "service1";
    const IdString_t INSTANCE1 = "instance1";
    const IdString_t INSTANCE2 = "instance2";
    const IdString_t EVENT1 = "event1";
    const IdString_t EVENT2 = "event2";
    const IdString_t EVENT3 = "event3";

    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_1_1(SERVICE1, INSTANCE1, EVENT1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_1_2(SERVICE1, INSTANCE1, EVENT2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_2_1(SERVICE1, INSTANCE2, EVENT1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_2_2(SERVICE1, INSTANCE2, EVENT2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_2_3(SERVICE1, INSTANCE2, EVENT3);

    iox::popo::UntypedPublisher publisher_sd_1_1_1(SERVICE_DESCRIPTION_1_1_1);
    iox::popo::UntypedPublisher publisher_sd_1_1_2(SERVICE_DESCRIPTION_1_1_2);
    iox::popo::UntypedPublisher publisher_sd_1_2_1(SERVICE_DESCRIPTION_1_2_1);
    iox::popo::UntypedPublisher publisher_sd_1_2_2(SERVICE_DESCRIPTION_1_2_2);
    iox::popo::UntypedPublisher publisher_sd_1_2_3(SERVICE_DESCRIPTION_1_2_3);

    auto serviceContainer = sut.findService(SERVICE1, INSTANCE1, EVENT1);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION_1_1_1));
    sut.findService(SERVICE1, INSTANCE1, EVENT1, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE1, INSTANCE1, EVENT2);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION_1_1_2));
    sut.findService(SERVICE1, INSTANCE1, EVENT2, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE1, INSTANCE2, EVENT1);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION_1_2_1));
    sut.findService(SERVICE1, INSTANCE2, EVENT1, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE1, INSTANCE2, EVENT2);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION_1_2_2));
    sut.findService(SERVICE1, INSTANCE2, EVENT2, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE1, INSTANCE2, EVENT3);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION_1_2_3));
    sut.findService(SERVICE1, INSTANCE2, EVENT3, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

TEST_F(ServiceDiscovery_test, FindServiceDoesNotReturnNotOfferedServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "e4f99eb1-7496-4a1e-bbd1-ebdb07e1ec9b");

    const IdString_t INSTANCE = "instance";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3("service3", INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd3(SERVICE_DESCRIPTION3);

    publisher_sd1.stopOffer();
    publisher_sd3.stopOffer();
    this->InterOpWait();

    auto serviceContainer = sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE_DESCRIPTION1.getServiceIDString(), INSTANCE, EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(SERVICE_DESCRIPTION2.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    ASSERT_THAT(serviceContainer.value().size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.value().begin(), Eq(SERVICE_DESCRIPTION2));
    sut.findService(SERVICE_DESCRIPTION2.getServiceIDString(), INSTANCE, EVENT, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);

    serviceContainer = sut.findService(SERVICE_DESCRIPTION3.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE_DESCRIPTION3.getServiceIDString(), INSTANCE, EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());
}

TEST_F(ServiceDiscovery_test, NonExistingServicesAreNotFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "86b87264-4df4-4d20-9357-06391ca1d57f");
    const IdString_t SERVICE = "service";
    const IdString_t NONEXISTENT_SERVICE = "ignatz";
    const IdString_t INSTANCE = "instance";
    const IdString_t NONEXISTENT_INSTANCE = "schlomo";
    const IdString_t EVENT = "event";
    const IdString_t NONEXISTENT_EVENT = "hypnotoad";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION(SERVICE, INSTANCE, EVENT);
    iox::popo::UntypedPublisher publisher_sd(SERVICE_DESCRIPTION);

    auto serviceContainer = sut.findService(NONEXISTENT_SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(NONEXISTENT_SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(NONEXISTENT_SERVICE, NONEXISTENT_INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(NONEXISTENT_SERVICE, NONEXISTENT_INSTANCE, EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(NONEXISTENT_SERVICE, INSTANCE, NONEXISTENT_EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(NONEXISTENT_SERVICE, INSTANCE, NONEXISTENT_EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(NONEXISTENT_SERVICE, INSTANCE, EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(NONEXISTENT_SERVICE, INSTANCE, EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());

    serviceContainer = sut.findService(SERVICE, INSTANCE, NONEXISTENT_EVENT);
    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(0U));
    sut.findService(SERVICE, INSTANCE, NONEXISTENT_EVENT, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());
}

TEST_F(ServiceDiscovery_test, FindServiceReturnsMaxServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "68628cc2-df6d-46e4-8586-7563f43bf10c");
    const IdString_t SERVICE = "s";
    ServiceContainer serviceContainerExp;
    iox::cxx::vector<iox::popo::UntypedPublisher, iox::MAX_NUMBER_OF_SERVICES> publishers;
    for (size_t i = 0; i < iox::MAX_NUMBER_OF_SERVICES; i++)
    {
        // Service & Instance string is kept short , to reduce the response size in find service request ,
        // (message queue has a limit of 512)
        std::string instance = "i" + iox::cxx::convert::toString(i);
        iox::capro::ServiceDescription SERVICE_DESCRIPTION(
            SERVICE, IdString_t(iox::cxx::TruncateToCapacity, instance), "foo");
        publishers.emplace_back(SERVICE_DESCRIPTION);
        serviceContainerExp.push_back(SERVICE_DESCRIPTION);
    }

    auto serviceContainer = sut.findService(SERVICE, iox::capro::Wildcard, iox::capro::Wildcard);

    ASSERT_FALSE(serviceContainer.has_error());
    EXPECT_THAT(serviceContainer.value().size(), Eq(iox::MAX_NUMBER_OF_SERVICES));
    EXPECT_TRUE(serviceContainer.value() == serviceContainerExp);

    sut.findService(SERVICE, iox::capro::Wildcard, iox::capro::Wildcard, findHandler);
    compareServiceContainers(serviceContainer.value(), searchResultOfFindServiceWithFindHandler);
}

/// @todo #415 #1074 this test is affected by the limits we set for service discovery,
/// if the container capacity is larger than what we can send with sockets
/// we will never cause an overflow
TEST_F(ServiceDiscovery_test, FindServiceReturnsContainerOverflowErrorWhenMoreThanMaxServicesAreFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "f2f8d8c0-8712-4e7a-9e33-2b2a918f8a71");
    const IdString_t SERVICE = "s";
    const size_t numberOfInstances = (iox::MAX_NUMBER_OF_SERVICES + 1);
    iox::cxx::vector<iox::popo::UntypedPublisher, numberOfInstances> publishers;
    for (size_t i = 0; i < numberOfInstances; i++)
    {
        std::string instance = "i" + iox::cxx::convert::toString(i);
        iox::capro::ServiceDescription SERVICE_DESCRIPTION(
            SERVICE, IdString_t(iox::cxx::TruncateToCapacity, instance), "foo");
        publishers.emplace_back(SERVICE_DESCRIPTION);
    }

    auto serviceContainer = sut.findService(SERVICE, iox::capro::Wildcard, iox::capro::Wildcard);
    ASSERT_THAT(serviceContainer.has_error(), Eq(true));

    sut.findService(SERVICE, iox::capro::Wildcard, iox::capro::Wildcard, findHandler);
    EXPECT_TRUE(searchResultOfFindServiceWithFindHandler.empty());
}

TEST_F(ServiceDiscovery_test, FindServiceWithEmptyCallableDoesNotDie)
{
    ::testing::Test::RecordProperty("TEST_ID", "7e1bf253-ce81-47cc-9b4a-605de7e49b64");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("ninjababy", "pow", "pow");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);
    iox::cxx::function_ref<void(const ServiceContainer&)> searchFunction;
    sut.findService(iox::capro::Wildcard, iox::capro::Wildcard, iox::capro::Wildcard, searchFunction);
}

} // namespace