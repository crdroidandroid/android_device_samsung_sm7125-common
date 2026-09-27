/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>

#include "ImsNew.h"

#include "msg/SipMsgUtil.h"

#include "ISipHeader.h"
#include "SipClientTransactionState.h"
#include "SipMethod.h"
#include "SipStack.h"

namespace android
{

class SipClientTransactionStateTest : public ::testing::Test
{
public:
    inline SipClientTransactionStateTest() :
            m_pCtState(IMS_NULL)
    {
    }

protected:
    void SetUp() override
    {
        SipMsgUtil::Init();

        m_pCtState = new SipClientTransactionState(IMS_SLOT_0);
        m_pCtState->AddReference();
    }

    void TearDown() override
    {
        if (m_pCtState != IMS_NULL)
        {
            m_pCtState->RemoveReference();
        }
    }

protected:
    SipClientTransactionState* m_pCtState;
};

TEST_F(SipClientTransactionStateTest, UpdateRouteDetails_StrictRouting_RemovesTopmostHeader)
{
    // Given a message with multiple Route headers, where the first is a strict router (no ;lr)
    ::SipMessage* pSipMsg = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    const AString strReqUri("<sip:user@ims.com>");
    SipAddrSpec* pUri = SipStack::DecodeAddrSpec(strReqUri);
    SipStack::SetRequestUri(pUri, pSipMsg);
    SipStack::FreeAddrSpec(pUri);

    AString strRoute1("sip:p1@ims.com");  // Strict router
    AString strRoute2("<sip:p2@ims.com;lr>");

    SipHeaderBase* pHdr1 =
            SipStack::DecodeHeader(SipHeaderBase::ROUTE, AString::ConstNull(), strRoute1);
    SipStack::AppendHeader(pHdr1, pSipMsg);
    SipStack::FreeHeader(pHdr1);

    SipHeaderBase* pHdr2 =
            SipStack::DecodeHeader(SipHeaderBase::ROUTE, AString::ConstNull(), strRoute2);
    SipStack::AppendHeader(pHdr2, pSipMsg);
    SipStack::FreeHeader(pHdr2);

    m_pCtState->UpdateMessage(pSipMsg);
    SipStack::FreeMessage(pSipMsg);

    // When UpdateRouteDetails is called
    IMS_BOOL bResult = m_pCtState->UpdateRouteDetails(SipMethod(SipMethod::INVITE));

    // Then it should succeed
    EXPECT_TRUE(bResult);
    EXPECT_TRUE(m_pCtState->IsTargetUpdated());

    // The topmost Route header should be removed (because it was strict) and
    // the Request-URI is added at the end of Route header list
    ::SipMessage* pUpdatedSipMsg = m_pCtState->GetMessage();
    EXPECT_EQ(2, SipStack::GetHeaderCount(pUpdatedSipMsg, ISipHeader::ROUTE));

    // Route index: 0
    AString strValue = SipStack::GetHeaderAsString(pUpdatedSipMsg, ISipHeader::ROUTE, IMS_TRUE, 0);
    EXPECT_EQ(strRoute2, strValue);

    // Route index: 1
    strValue = SipStack::GetHeaderAsString(pUpdatedSipMsg, ISipHeader::ROUTE, IMS_TRUE, 1);
    EXPECT_EQ(strReqUri, strValue);

    pUri = SipStack::GetRequestUri(pUpdatedSipMsg);
    ASSERT_NE(pUri, IMS_NULL);

    // Request-URI
    strValue = AString::ConstNull();
    SipStack::EncodeAddrSpec(pUri, IMS_TRUE, strValue);
    EXPECT_EQ(strRoute1, strValue);

    SipStack::FreeAddrSpec(pUri);
}

TEST_F(SipClientTransactionStateTest, UpdateRouteDetails_LooseRouting_PreservesRouteHeader)
{
    // Given a message with multiple Route headers, where the first is a loose router (;lr)
    ::SipMessage* pSipMsg = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    AString strReqUri("sip:user@ims.com");
    SipAddrSpec* pUri = SipStack::DecodeAddrSpec(strReqUri);
    SipStack::SetRequestUri(pUri, pSipMsg);
    SipStack::FreeAddrSpec(pUri);

    const AString strRoute1("<sip:p1@ims.com;lr>");
    const AString strRoute2("<sip:p2@ims.com;lr>");

    SipHeaderBase* pHdr1 =
            SipStack::DecodeHeader(SipHeaderBase::ROUTE, AString::ConstNull(), strRoute1);
    SipStack::AppendHeader(pHdr1, pSipMsg);
    SipStack::FreeHeader(pHdr1);

    SipHeaderBase* pHdr2 =
            SipStack::DecodeHeader(SipHeaderBase::ROUTE, AString::ConstNull(), strRoute2);
    SipStack::AppendHeader(pHdr2, pSipMsg);
    SipStack::FreeHeader(pHdr2);

    m_pCtState->UpdateMessage(pSipMsg);
    SipStack::FreeMessage(pSipMsg);

    // When UpdateRouteDetails is called
    IMS_BOOL bResult = m_pCtState->UpdateRouteDetails(SipMethod(SipMethod::INVITE));

    // Then it should succeed
    EXPECT_TRUE(bResult);
    EXPECT_FALSE(m_pCtState->IsTargetUpdated());

    // And the topmost Route header should be PRESERVED (because it was loose)
    ::SipMessage* pUpdatedSipMsg = m_pCtState->GetMessage();
    EXPECT_EQ(2, SipStack::GetHeaderCount(pUpdatedSipMsg, ISipHeader::ROUTE));

    // Route index: 0
    AString strValue = SipStack::GetHeaderAsString(pUpdatedSipMsg, ISipHeader::ROUTE, IMS_TRUE, 0);
    EXPECT_EQ(strRoute1, strValue);

    // Route index: 1
    strValue = SipStack::GetHeaderAsString(pUpdatedSipMsg, ISipHeader::ROUTE, IMS_TRUE, 1);
    EXPECT_EQ(strRoute2, strValue);

    pUri = SipStack::GetRequestUri(pUpdatedSipMsg);
    ASSERT_NE(pUri, IMS_NULL);

    // Request-URI
    strValue = AString::ConstNull();
    SipStack::EncodeAddrSpec(pUri, IMS_TRUE, strValue);
    EXPECT_EQ(strReqUri, strValue);

    SipStack::FreeAddrSpec(pUri);
}

}  // namespace android
