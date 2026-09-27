/**
 * Copyright (C) 2022 The Android Open Source Project
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

package android.telephony.imsmedia;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.net.DatagramSocket;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

public class ImsMediaManagerTest {
    private static final int WAIT_TIMEOUT_MS = 1000;
    private Executor mExecutor;
    private CountDownLatch mConnectionLatch;
    private CountDownLatch mDisconnectLatch;

    @Mock
    Context mMockContext;
    @Mock
    DatagramSocket mMockRtpSocket, mMockRtcpSocket;
    @Mock
    ImsMediaManager.OnConnectedCallback mMockConnectedCallback;
    @Mock
    RtpConfig mMockRtpConfig;
    @Mock
    ImsMediaSession mMockImsMediaSession;
    @Mock
    ImsMediaManager.SessionCallback mMockSessionCallback;
    @Mock
    IImsMedia mMockImsMedia;
    @Mock
    IBinder mMockBinder;
    @Mock
    ImsMediaManager.ImsMediaManagerCallback mMockImsMediaManagerCallback;

    @Before
    public void setUp() throws Exception {
        mExecutor = Executors.newSingleThreadExecutor();
        mConnectionLatch = new CountDownLatch(1);
        mDisconnectLatch = new CountDownLatch(1);

        MockitoAnnotations.initMocks(this);
        doReturn(mMockBinder).when(mMockImsMedia).asBinder();
        doReturn(mMockImsMedia).when(mMockBinder).queryLocalInterface(any(String.class));
        doReturn(mMockBinder).when(mMockImsMediaSession).getBinder();

        doAnswer(invocation -> {
            mConnectionLatch.countDown();
            return null;
        }).when(mMockConnectedCallback).onConnected();

        doAnswer(invocation -> {
            mDisconnectLatch.countDown();
            return null;
        }).when(mMockConnectedCallback).onDisconnected();

        doAnswer(new Answer<Boolean>() {
            @Override
            public Boolean answer(InvocationOnMock invocation) throws Throwable {
                Object[] args = invocation.getArguments();
                boolean bServiceConnectionFound = false;
                boolean bIntentFound = false;
                for (Object arg : args) {
                    if (arg instanceof ServiceConnection) {
                        synchronized (arg) {
                            bServiceConnectionFound = true;
                            ((ServiceConnection) arg).onServiceConnected(null,
                                    mMockBinder);
                            arg.notify();
                        }
                    } else if (arg instanceof Intent) {
                        bIntentFound = true;
                        Intent intent = (Intent) arg;
                        assertEquals(intent.getAction(), IImsMedia.class.getName());
                        assertEquals(intent.getComponent(), ComponentName.createRelative(
                                ImsMediaManager.MEDIA_SERVICE_PACKAGE,
                                ImsMediaManager.MEDIA_SERVICE_CLASS));
                    }
                }

                assertEquals(bServiceConnectionFound, true);
                assertEquals(bIntentFound, true);
                return true;
            }
        }).when(mMockContext).bindService(any(Intent.class), any(ServiceConnection.class),
                eq(Context.BIND_AUTO_CREATE));

        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                Object[] args = invocation.getArguments();
                for (Object arg : args) {
                    if (arg instanceof ServiceConnection) {
                        synchronized (arg) {
                            ((ServiceConnection) arg).onServiceDisconnected(null);
                            arg.notify();
                        }
                    }
                }
                return null;
            }
        }).when(mMockContext).unbindService(any(ServiceConnection.class));
    }

    @Test
    public void testServiceBindingAndUnbinding() {
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);

        waitForConnection();

        //Unbind from service
        imsMediaManager.release();

        waitForDisconnect();

        verify(mMockConnectedCallback).onConnected();
        verify(mMockConnectedCallback).onDisconnected();
    }

    @Test
    public void testOpenAndCloseSession() {
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);

        waitForConnection();

        imsMediaManager.openSession(mMockRtpSocket, mMockRtcpSocket,
                ImsMediaSession.SESSION_TYPE_AUDIO, mMockRtpConfig, mExecutor,
                mMockSessionCallback);

        imsMediaManager.closeSession(mMockImsMediaSession);

        //Unbind from service
        imsMediaManager.release();

        waitForDisconnect();

        try {
            verify(mMockConnectedCallback).onConnected();
            verify(mMockImsMedia).closeSession(mMockBinder);
            verify(mMockConnectedCallback).onDisconnected();
        } catch (Exception e) {
            fail(e.getMessage());
        }
    }

    @Test
    public void testConstructorWithNulls() {
        try {
            new ImsMediaManager(null, mExecutor, mMockConnectedCallback);
            fail("Should have thrown NullPointerException for null context");
        } catch (NullPointerException e) {
            // expected
        }
        try {
            new ImsMediaManager(mMockContext, null, mMockConnectedCallback);
            fail("Should have thrown NullPointerException for null executor");
        } catch (NullPointerException e) {
            // expected
        }
        try {
            new ImsMediaManager(mMockContext, mExecutor, null);
            fail("Should have thrown NullPointerException for null callback");
        } catch (NullPointerException e) {
            // expected
        }
    }

    @SuppressWarnings("unused")
    @Test
    public void testServiceBindingFailure() {
        doReturn(false).when(mMockContext).bindService(any(Intent.class),
                any(ServiceConnection.class), eq(Context.BIND_AUTO_CREATE));
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);
        try {
            mConnectionLatch.await(100, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.getMessage());
        }
        verify(mMockConnectedCallback, times(0)).onConnected();
    }

    @Test
    public void testOperationsWhenNotConnected() throws RemoteException {
        doReturn(false).when(mMockContext).bindService(any(Intent.class),
                any(ServiceConnection.class), eq(Context.BIND_AUTO_CREATE));
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);

        imsMediaManager.openSession(mMockRtpSocket, mMockRtcpSocket,
                ImsMediaSession.SESSION_TYPE_AUDIO, mMockRtpConfig, mExecutor,
                mMockSessionCallback);
        imsMediaManager.closeSession(mMockImsMediaSession);
        imsMediaManager.generateVideoSprop(new VideoConfig[]{}, mMockImsMediaManagerCallback);
        imsMediaManager.setTestMode(1);
        imsMediaManager.release();

        verify(mMockImsMedia, times(0)).openSession(any(), any(), anyInt(), any(), any());
        verify(mMockImsMedia, times(0)).closeSession(any());
        verify(mMockImsMedia, times(0)).generateVideoSprop(any(), any());
        verify(mMockImsMedia, times(0)).setTestMode(anyInt());
    }

    @Test
    public void testRemoteExceptions() throws RemoteException {
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);
        waitForConnection();

        doThrow(new RemoteException()).when(mMockImsMedia).openSession(any(), any(), anyInt(),
                any(), any());
        imsMediaManager.openSession(mMockRtpSocket, mMockRtcpSocket,
                ImsMediaSession.SESSION_TYPE_AUDIO, mMockRtpConfig, mExecutor,
                mMockSessionCallback);

        doThrow(new RemoteException()).when(mMockImsMedia).closeSession(any());
        imsMediaManager.closeSession(mMockImsMediaSession);

        doThrow(new RemoteException()).when(mMockImsMedia).generateVideoSprop(any(), any());
        imsMediaManager.generateVideoSprop(new VideoConfig[]{}, mMockImsMediaManagerCallback);

        doThrow(new RemoteException()).when(mMockImsMedia).setTestMode(anyInt());
        imsMediaManager.setTestMode(1);
    }

    @Test
    public void testGenerateVideoSprop() throws RemoteException {
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);
        waitForConnection();

        // VideoConfig is a final class, so we create a real instance instead of mocking it.
        VideoConfig videoConfig = new VideoConfig.Builder().build();
        VideoConfig[] videoConfigs = new VideoConfig[]{videoConfig};
        String[] spropList = new String[]{"sprop1"};

        doAnswer(invocation -> {
            IImsMediaCallback callback = invocation.getArgument(1);
            callback.onVideoSpropResponse(spropList);
            return null;
        }).when(mMockImsMedia).generateVideoSprop(eq(videoConfigs), any());

        imsMediaManager.generateVideoSprop(videoConfigs, mMockImsMediaManagerCallback);

        verify(mMockImsMediaManagerCallback).onVideoSpropResponse(spropList);
    }

    @Test
    public void testSetTestMode() throws RemoteException {
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);
        waitForConnection();

        int testMode = 123;
        imsMediaManager.setTestMode(testMode);
        verify(mMockImsMedia).setTestMode(testMode);
    }

    @Test
    public void testReleaseThrowsIllegalArgumentException() {
        ImsMediaManager imsMediaManager =
                new ImsMediaManager(mMockContext, mExecutor, mMockConnectedCallback);
        waitForConnection();

        doThrow(new IllegalArgumentException()).when(mMockContext).unbindService(
                any(ServiceConnection.class));
        imsMediaManager.release();
        verify(mMockConnectedCallback, times(0)).onDisconnected();
    }

    @Test
    public void testSessionCallbackDefaultMethods() {
        ImsMediaManager.SessionCallback callback = new ImsMediaManager.SessionCallback() {
        };
        callback.setExecutor(mExecutor);
        callback.onOpenSessionSuccess(mMockImsMediaSession);
        callback.onOpenSessionFailure(0);
        callback.onSessionClosed();
    }

    private void waitForConnection() {
        try {
            mConnectionLatch.await(WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.getMessage());
        }
    }

    private void waitForDisconnect() {
        try {
            mDisconnectLatch.await(WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail(e.getMessage());
        }
    }
}
