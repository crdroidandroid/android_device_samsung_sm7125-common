/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "ImsStrLib.h"

#include "SipTimerContext.h"
#include "msg/SipMsgUtil.h"
#include "txn/SipTxn.h"
#include "txn/SipTxnTimerValues.h"

#include "SipStack.h"

namespace android
{

class SipStackTest : public ::testing::Test
{
protected:
    virtual void SetUp() override { SipMsgUtil::Init(); }

    virtual void TearDown() override {}
};

TEST_F(SipStackTest, DisplayBadHeaders)
{
    const SIP_CHAR acMsg[] = {"INVITE sip:user@example.com SIP/2.0\r\n"
                              "Max-Forwards: 254\r\n"
                              "To: sip:j.user@example.com\r\n"
                              "From: sip:caller@example.net;tag=32394234\r\n"
                              "Call-ID: ncl.0ha0isndaksdj2193423r542w35\r\n"
                              "CSeq: 0 INVITE\r\n"
                              "Via: SIP/2.0/UDP 192.0.2.53;branch=z9hG4bKkdjuw\r\n"
                              "Contact: sip:caller@example53.example.net\r\n"
                              "Date: Fri, 01 Jan 2010 16:00:00 EST\r\n"
                              "Content-Length: 0\r\n"
                              "\r\n"};

    ::SipMessage* pSipMsg = new ::SipMessage();
    SIP_BOOL bResult = pSipMsg->Decode(acMsg, IMS_StrLen(acMsg));

    EXPECT_EQ(SIP_TRUE, bResult);
    EXPECT_EQ(1, pSipMsg->GetBadHeaderCount());  // Date is a bad header.

    SipStack::DisplayBadHeaders(pSipMsg);

    // Checks whether the bad header list is kept or not.
    EXPECT_EQ(1, pSipMsg->GetBadHeaderCount());

    pSipMsg->SipDelete();
}

TEST_F(SipStackTest, SetTimerValues)
{
    SipTimerValues objTv;
    IMS_UINT32 nT1 = 3000;
    IMS_UINT32 nT2 = 16000;
    IMS_UINT32 nTB = 32000;
    IMS_UINT32 nTD = 64000;
    IMS_UINT32 nTF = 8000;
    IMS_UINT32 nTH = 192000;
    IMS_UINT32 nTI = 64000;
    IMS_UINT32 nTJ = 64000;
    IMS_UINT32 nTK = 32000;

    objTv.SetValue(SipTimerValues::TIMER_T1, nT1);
    objTv.SetValue(SipTimerValues::TIMER_T2, nT2);
    objTv.SetValue(SipTimerValues::TIMER_B, nTB);
    objTv.SetValue(SipTimerValues::TIMER_D, nTD);
    objTv.SetValue(SipTimerValues::TIMER_F, nTF);
    objTv.SetValue(SipTimerValues::TIMER_H, nTH);
    objTv.SetValue(SipTimerValues::TIMER_I, nTI);
    objTv.SetValue(SipTimerValues::TIMER_J, nTJ);
    objTv.SetValue(SipTimerValues::TIMER_K, nTK);

    SipTxnContext objTxnContext;
    const SipTxnTimerValues* pTxnTimerValues = objTxnContext.m_pSipTimerContext->m_pTxnSipTxnTimers;
    SipTxnContext* pTxnContext = &objTxnContext;
    SipStack::SetTimerValues(&objTv, pTxnContext);

    EXPECT_EQ(nT1, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_T1));
    EXPECT_EQ(nT1, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_A));
    EXPECT_EQ(nT1, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_E));
    EXPECT_EQ(nT1, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_G));
    EXPECT_EQ(nT1 * 64, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_M));

    EXPECT_EQ(nT2, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_T2));
    EXPECT_EQ(nT2 + 1000, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_T4));

    EXPECT_EQ(nTB, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_B));
    EXPECT_EQ(nTD, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_D));
    EXPECT_EQ(nTF, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_F));
    EXPECT_EQ(nTH, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_H));
    EXPECT_EQ(nTI, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_I));
    EXPECT_EQ(nTJ, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_J));
    EXPECT_EQ(nTK, pTxnTimerValues->GetTimerValue(SipTxn::TIMER_K));
}

TEST_F(SipStackTest, AppendHeader_WithNullHeader_SetsInvalidParamError)
{
    // This test verifies that calling a public API with invalid parameters
    // correctly triggers the internal SIPStackError function, and the error
    // can be retrieved via GetLastError.
    ::SipMessage* pMessage = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    ASSERT_NE(pMessage, nullptr);

    // When AppendHeader is called with a null header
    IMS_BOOL bResult = SipStack::AppendHeader(nullptr, pMessage);

    // Then the operation should fail
    EXPECT_FALSE(bResult);
    // And the last error should be EERR_INVALIDPARAM
    EXPECT_EQ(SipStack::GetLastError(), EERR_INVALIDPARAM);

    SipStack::FreeMessage(pMessage);
}

TEST_F(SipStackTest, EncodeAddrSpec_AddressWithoutParameters_EncodesUriOnly)
{
    // Given an address specification without any parameters
    const char* pszSipUri = "sip:user@example.com";
    SipAddrSpec* pAddrSpec = SipStack::DecodeAddrSpec(AString(pszSipUri));
    ASSERT_NE(pAddrSpec, nullptr);
    AString strEncodedAddr;

    // When encoding with the parameters flag set to false
    IMS_BOOL bResult = SipStack::EncodeAddrSpec(pAddrSpec, IMS_FALSE, strEncodedAddr);

    // Then the encoding succeeds and contains only the base URI
    EXPECT_TRUE(bResult);
    EXPECT_STREQ(pszSipUri, strEncodedAddr.GetStr());

    // When encoding with the parameters flag set to true (but no params exist)
    bResult = SipStack::EncodeAddrSpec(pAddrSpec, IMS_TRUE, strEncodedAddr);

    // Then the encoding still succeeds and contains only the base URI
    EXPECT_TRUE(bResult);
    EXPECT_STREQ(pszSipUri, strEncodedAddr.GetStr());

    SipStack::FreeAddrSpec(pAddrSpec);
}

TEST_F(SipStackTest, EncodeAddrSpec_AddressWithParameters_EncodesUriWithParameter)
{
    // Given an address specification with parameters
    const char* pszSipUri = "sip:user@example.com";
    const char* pszSipUriWithParam = "sip:user@example.com;transport=udp";
    SipAddrSpec* pAddrSpec = SipStack::DecodeAddrSpec(AString(pszSipUriWithParam));
    ASSERT_NE(pAddrSpec, nullptr);
    AString strEncodedAddr;

    // When encoding with the parameters flag set to false
    IMS_BOOL bResult = SipStack::EncodeAddrSpec(pAddrSpec, IMS_FALSE, strEncodedAddr);

    // Then the encoding succeeds and does not include the parameters
    EXPECT_TRUE(bResult);
    EXPECT_STREQ(pszSipUri, strEncodedAddr.GetStr());

    // When encoding with the parameters flag set to true
    bResult = SipStack::EncodeAddrSpec(pAddrSpec, IMS_TRUE, strEncodedAddr);

    // Then the encoding succeeds and includes the parameters
    EXPECT_TRUE(bResult);
    EXPECT_STREQ(pszSipUriWithParam, strEncodedAddr.GetStr());

    SipStack::FreeAddrSpec(pAddrSpec);
}

TEST_F(SipStackTest, EncodeAddrSpec_NullAddress_ReturnsFalseAndSetsError)
{
    // Given a null address specification
    AString strEncodedAddr;

    // When trying to encode it
    IMS_BOOL bResult = SipStack::EncodeAddrSpec(nullptr, IMS_TRUE, strEncodedAddr);

    // Then the operation fails and the correct error is set
    EXPECT_FALSE(bResult);
    EXPECT_EQ(SipStack::GetLastError(), EERR_INVALIDPARAM);
}

TEST_F(SipStackTest, CorrectMessageBody_MultipartWithoutBoundary_AddsBoundaryParameter)
{
    // Given a message with multiple bodies and a Content-Type header without a boundary
    ::SipMessage* pMessage = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    ASSERT_NE(pMessage, nullptr);
    SipMsgBody* pBody1 = SipStack::CreateMessageBody();
    SipStack::AppendMessageBody(pBody1, pMessage);
    SipStack::FreeMessageBody(pBody1);
    SipMsgBody* pBody2 = SipStack::CreateMessageBody();
    SipStack::AppendMessageBody(pBody2, pMessage);
    SipStack::FreeMessageBody(pBody2);

    SipHeaderBase* pContentType = SipStack::DecodeHeader(
            SipHeaderBase::CONTENT_TYPE, AString::ConstNull(), AString("multipart/mixed"));
    ASSERT_NE(pContentType, nullptr);
    SipStack::SetHeader(pContentType, pMessage);
    SipStack::FreeHeader(pContentType);

    // When CorrectMessageBody is called
    IMS_BOOL bResult = SipStack::CorrectMessageBody(pMessage);
    EXPECT_TRUE(bResult);

    // Then a boundary parameter should be added to the Content-Type header
    SipHeaderBase* pHeader = SipStack::GetHeader(pMessage, SipHeaderBase::CONTENT_TYPE);
    ASSERT_NE(pHeader, nullptr);
    AString strBoundary = SipStack::GetParameter(pHeader, AString("boundary"));
    EXPECT_NE(0, strBoundary.GetLength());
    SipStack::FreeHeader(pHeader);

    SipStack::FreeMessage(pMessage);
}

TEST_F(SipStackTest, CorrectMessageBody_MultipartWithBoundary_PreservesBoundaryParameter)
{
    // Given a message with multiple bodies and a Content-Type header that already has a boundary
    ::SipMessage* pMessage = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    ASSERT_NE(pMessage, nullptr);
    SipMsgBody* pBody1 = SipStack::CreateMessageBody();
    SipStack::AppendMessageBody(pBody1, pMessage);
    SipStack::FreeMessageBody(pBody1);
    SipMsgBody* pBody2 = SipStack::CreateMessageBody();
    SipStack::AppendMessageBody(pBody2, pMessage);
    SipStack::FreeMessageBody(pBody2);

    const char* pszContentTypeWithBoundary = "multipart/mixed;boundary=myboundary";
    SipHeaderBase* pContentType = SipStack::DecodeHeader(
            SipHeaderBase::CONTENT_TYPE, AString::ConstNull(), AString(pszContentTypeWithBoundary));
    ASSERT_NE(pContentType, nullptr);
    SipStack::SetHeader(pContentType, pMessage);
    SipStack::FreeHeader(pContentType);

    // When CorrectMessageBody is called
    IMS_BOOL bResult = SipStack::CorrectMessageBody(pMessage);
    EXPECT_TRUE(bResult);

    // Then the existing boundary parameter should be preserved
    SipHeaderBase* pHeader = SipStack::GetHeader(pMessage, SipHeaderBase::CONTENT_TYPE);
    ASSERT_NE(pHeader, nullptr);
    AString strBoundary = SipStack::GetParameter(pHeader, AString("boundary"));
    EXPECT_STREQ("myboundary", strBoundary.GetStr());
    SipStack::FreeHeader(pHeader);

    SipStack::FreeMessage(pMessage);
}

TEST_F(SipStackTest, RemoveHeader_WithIndex_RemovesCorrectHeader)
{
    // Given a message with multiple Route headers
    ::SipMessage* pMessage = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    ASSERT_NE(pMessage, nullptr);
    EXPECT_EQ(pMessage->GetHdrList(SipHeaderBase::ROUTE), nullptr);

    const AString strRoute1("<sip:p1@ims.com;lr>");
    const AString strRoute2("<sip:p2@ims.com;lr>");

    SipHeaderBase* pHdr1 =
            SipStack::DecodeHeader(SipHeaderBase::ROUTE, AString::ConstNull(), strRoute1);
    SipStack::AppendHeader(pHdr1, pMessage);
    SipStack::FreeHeader(pHdr1);

    SipHeaderBase* pHdr2 =
            SipStack::DecodeHeader(SipHeaderBase::ROUTE, AString::ConstNull(), strRoute2);
    SipStack::AppendHeader(pHdr2, pMessage);
    SipStack::FreeHeader(pHdr2);

    EXPECT_EQ(2, SipStack::GetHeaderCount(pMessage, SipHeaderBase::ROUTE));

    // When RemoveHeader is called with index 0
    SipStack::RemoveHeader(SipHeaderBase::ROUTE, 0, pMessage);

    // Then the first Route header is removed and the second remains
    EXPECT_EQ(1, SipStack::GetHeaderCount(pMessage, SipHeaderBase::ROUTE));
    EXPECT_NE(pMessage->GetHdrList(SipHeaderBase::ROUTE), nullptr);

    AString strValue = SipStack::GetHeaderAsString(pMessage, SipHeaderBase::ROUTE, IMS_TRUE);
    EXPECT_EQ(strRoute2, strValue);

    // When RemoveHeader is called with index 0
    SipStack::RemoveHeader(SipHeaderBase::ROUTE, 0, pMessage);

    // No Route headers
    EXPECT_EQ(0, SipStack::GetHeaderCount(pMessage, SipHeaderBase::ROUTE));
    EXPECT_EQ(pMessage->GetHdrList(SipHeaderBase::ROUTE), nullptr);

    SipStack::FreeMessage(pMessage);
}

TEST_F(SipStackTest, RemoveHeader_WithIndex_NonExistentHeader_DoesNothing)
{
    // Given a message without any Route headers
    ::SipMessage* pMessage = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    ASSERT_NE(pMessage, nullptr);

    // When RemoveHeader is called for Route header
    SipStack::RemoveHeader(SipHeaderBase::ROUTE, 0, pMessage);

    // Then the message remains unchanged and no crash occurs
    EXPECT_EQ(0, SipStack::GetHeaderCount(pMessage, SipHeaderBase::ROUTE));

    SipStack::FreeMessage(pMessage);
}

TEST_F(SipStackTest, RemoveHeader_WithIndex_SingleHeader_RemovesHeader)
{
    // Given a message with a non-list header (e.g., To)
    ::SipMessage* pMessage = SipStack::CreateMessage(::SipMessage::REQ_TYPE);
    ASSERT_NE(pMessage, nullptr);
    const AString strTo("sip:user@ims.com");
    SipHeaderBase* pTo = SipStack::DecodeHeader(SipHeaderBase::TO, AString::ConstNull(), strTo);
    SipStack::SetHeader(pTo, pMessage);
    SipStack::FreeHeader(pTo);

    EXPECT_EQ(1, SipStack::GetHeaderCount(pMessage, SipHeaderBase::TO));

    // When RemoveHeader is called with index 0
    SipStack::RemoveHeader(SipHeaderBase::TO, 0, pMessage);

    // Then the header is removed
    EXPECT_EQ(0, SipStack::GetHeaderCount(pMessage, SipHeaderBase::TO));

    SipStack::FreeMessage(pMessage);
}

}  // namespace android
