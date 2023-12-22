package com.android.bluetooth.hfpclient;

import static com.android.bluetooth.hfpclient.HeadsetClientStateMachine.AT_OK;

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAssignedNumbers;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadsetClient;
import android.bluetooth.BluetoothHeadsetClientCall;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.media.AudioManager;
import android.os.HandlerThread;
import android.os.Message;

import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.R;
import com.android.bluetooth.Utils;

import org.hamcrest.core.AllOf;
import org.hamcrest.core.IsInstanceOf;
import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.hamcrest.MockitoHamcrest;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class HeadsetClientStateMachineTest {
    private BluetoothAdapter mAdapter;
    private HandlerThread mHandlerThread;
    private HeadsetClientStateMachine mHeadsetClientStateMachine;
    private BluetoothDevice mTestDevice;
    private Context mTargetContext;

    @Mock
    private Resources mMockHfpResources;
    @Mock
    private HeadsetClientService mHeadsetClientService;
    @Mock
    private AudioManager mAudioManager;

    private NativeInterface mNativeInterface;

    private static final int STANDARD_WAIT_MILLIS = 1000;
    private static final int QUERY_CURRENT_CALLS_WAIT_MILLIS = 2000;
    private static final int QUERY_CURRENT_CALLS_TEST_WAIT_MILLIS = QUERY_CURRENT_CALLS_WAIT_MILLIS
            * 3 / 2;

    @Before
    public void setUp() {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        Assume.assumeTrue("Ignore test when HeadsetClientService is not enabled",
                mTargetContext.getResources().getBoolean(R.bool.profile_supported_hfpclient));
        // Setup mocks and test assets
        MockitoAnnotations.initMocks(this);
        // Set a valid volume
        when(mAudioManager.getStreamVolume(anyInt())).thenReturn(2);
        when(mAudioManager.getStreamMaxVolume(anyInt())).thenReturn(10);
        when(mAudioManager.getStreamMinVolume(anyInt())).thenReturn(1);
        when(mHeadsetClientService.getAudioManager()).thenReturn(
                mAudioManager);
        when(mHeadsetClientService.getResources()).thenReturn(mMockHfpResources);
        when(mMockHfpResources.getBoolean(R.bool.hfp_clcc_poll_during_call)).thenReturn(true);
        mNativeInterface = spy(NativeInterface.getInstance());

        // This line must be called to make sure relevant objects are initialized properly
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        // Get a device for testing
        mTestDevice = mAdapter.getRemoteDevice("00:01:02:03:04:05");

        // Setup thread and looper
        mHandlerThread = new HandlerThread("HeadsetClientStateMachineTestHandlerThread");
        mHandlerThread.start();
        // Manage looper execution in main test thread explicitly to guarantee timing consistency
        mHeadsetClientStateMachine =
                new HeadsetClientStateMachine(mHeadsetClientService, mHandlerThread.getLooper(),
                                              mNativeInterface);
        mHeadsetClientStateMachine.start();
    }

    @After
    public void tearDown() {
        if (!mTargetContext.getResources().getBoolean(R.bool.profile_supported_hfpclient)) {
            return;
        }
        mHeadsetClientStateMachine.doQuit();
        mHandlerThread.quit();
    }

    /**
     * Test that default state is disconnected
     */
    @SmallTest
    @Test
    public void testDefaultDisconnectedState() {
        Assert.assertEquals(mHeadsetClientStateMachine.getConnectionState(null),
                BluetoothProfile.STATE_DISCONNECTED);
    }

    /**
     * Test that an incoming connection with low priority is rejected
     */
    @MediumTest
    @Test
    public void testIncomingPriorityReject() {
        // Return false for priority.
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_FORBIDDEN);

        // Inject an event for when incoming connection is requested
        StackEvent connStCh = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        connStCh.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_CONNECTED;
        connStCh.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, connStCh);

        // Verify that only DISCONNECTED -> DISCONNECTED broadcast is fired
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS)).sendBroadcast(MockitoHamcrest
                .argThat(
                AllOf.allOf(IntentMatchers.hasAction(
                        BluetoothHeadsetClient.ACTION_CONNECTION_STATE_CHANGED),
                        IntentMatchers.hasExtra(BluetoothProfile.EXTRA_STATE,
                                BluetoothProfile.STATE_DISCONNECTED),
                        IntentMatchers.hasExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE,
                                BluetoothProfile.STATE_DISCONNECTED))), anyString());
        // Check we are in disconnected state still.
        Assert.assertThat(mHeadsetClientStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(HeadsetClientStateMachine.Disconnected.class));
    }

    /**
     * Test that an incoming connection with high priority is accepted
     */
    @MediumTest
    @Test
    public void testIncomingPriorityAccept() {
        // Return true for priority.
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        // Inject an event for when incoming connection is requested
        StackEvent connStCh = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        connStCh.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_CONNECTED;
        connStCh.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, connStCh);

        // Verify that one connection state broadcast is executed
        ArgumentCaptor<Intent> intentArgument1 = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS)).sendBroadcast(intentArgument1
                .capture(),
                anyString());
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTING,
                intentArgument1.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));

        // Check we are in connecting state now.
        Assert.assertThat(mHeadsetClientStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(HeadsetClientStateMachine.Connecting.class));

        // Send a message to trigger SLC connection
        StackEvent slcEvent = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        slcEvent.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_SLC_CONNECTED;
        slcEvent.valueInt2 = HeadsetClientHalConstants.PEER_FEAT_ECS;
        slcEvent.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, slcEvent);

        // Verify that one connection state broadcast is executed
        ArgumentCaptor<Intent> intentArgument2 = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(2)).sendBroadcast(
                intentArgument2.capture(), anyString());
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTED,
                intentArgument2.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));
        // Check we are in connecting state now.
        Assert.assertThat(mHeadsetClientStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(HeadsetClientStateMachine.Connected.class));
    }

    /**
     * Test that an incoming connection that times out
     */
    @MediumTest
    @Test
    public void testIncomingTimeout() {
        // Return true for priority.
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        // Inject an event for when incoming connection is requested
        StackEvent connStCh = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        connStCh.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_CONNECTED;
        connStCh.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, connStCh);

        // Verify that one connection state broadcast is executed
        ArgumentCaptor<Intent> intentArgument1 = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS)).sendBroadcast(intentArgument1
                .capture(),
                anyString());
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTING,
                intentArgument1.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));

        // Check we are in connecting state now.
        Assert.assertThat(mHeadsetClientStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(HeadsetClientStateMachine.Connecting.class));

        // Verify that one connection state broadcast is executed
        ArgumentCaptor<Intent> intentArgument2 = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService,
                timeout(HeadsetClientStateMachine.CONNECTING_TIMEOUT_MS * 2).times(
                        2)).sendBroadcast(intentArgument2.capture(), anyString());
        Assert.assertEquals(BluetoothProfile.STATE_DISCONNECTED,
                intentArgument2.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));

        // Check we are in connecting state now.
        Assert.assertThat(mHeadsetClientStateMachine.getCurrentState(),
                IsInstanceOf.instanceOf(HeadsetClientStateMachine.Disconnected.class));
    }

    /**
     * Test that In Band Ringtone information is relayed from phone.
     */
    @LargeTest
    @Test
    public void testInBandRingtone() {
        // Return true for priority.
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        Assert.assertEquals(false, mHeadsetClientStateMachine.getInBandRing());

        // Inject an event for when incoming connection is requested
        StackEvent connStCh = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        connStCh.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_CONNECTED;
        connStCh.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, connStCh);

        // Verify that one connection state broadcast is executed
        ArgumentCaptor<Intent> intentArgument = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS)).sendBroadcast(intentArgument
                .capture(),
                anyString());
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTING,
                intentArgument.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));

        // Send a message to trigger SLC connection
        StackEvent slcEvent = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        slcEvent.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_SLC_CONNECTED;
        slcEvent.valueInt2 = HeadsetClientHalConstants.PEER_FEAT_ECS;
        slcEvent.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, slcEvent);

        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(2)).sendBroadcast(
                intentArgument.capture(),
                anyString());

        Assert.assertEquals(BluetoothProfile.STATE_CONNECTED,
                intentArgument.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));

        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_IN_BAND_RINGTONE);
        event.valueInt = 0;
        event.device = mTestDevice;

        // Enable In Band Ring and verify state gets propagated.
        StackEvent eventInBandRing = new StackEvent(StackEvent.EVENT_TYPE_IN_BAND_RINGTONE);
        eventInBandRing.valueInt = 1;
        eventInBandRing.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, eventInBandRing);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(3)).sendBroadcast(
                intentArgument.capture(),
                anyString());
        Assert.assertEquals(1,
                intentArgument.getValue().getIntExtra(BluetoothHeadsetClient.EXTRA_IN_BAND_RING,
                        -1));
        Assert.assertEquals(true, mHeadsetClientStateMachine.getInBandRing());


        // Simulate a new incoming phone call
        StackEvent eventCallStatusUpdated = new StackEvent(StackEvent.EVENT_TYPE_CLIP);
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, eventCallStatusUpdated);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(3)).sendBroadcast(
                intentArgument.capture(),
                anyString());

        // Provide information about the new call
        StackEvent eventIncomingCall = new StackEvent(StackEvent.EVENT_TYPE_CURRENT_CALLS);
        eventIncomingCall.valueInt = 1; //index
        eventIncomingCall.valueInt2 = 1; //direction
        eventIncomingCall.valueInt3 = 4; //state
        eventIncomingCall.valueInt4 = 0; //multi party
        eventIncomingCall.valueString = "5551212"; //phone number
        eventIncomingCall.device = mTestDevice;

        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, eventIncomingCall);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(3)).sendBroadcast(
                intentArgument.capture(),
                anyString());


        // Signal that the complete list of calls was received.
        StackEvent eventCommandStatus = new StackEvent(StackEvent.EVENT_TYPE_CMD_RESULT);
        eventCommandStatus.valueInt = AT_OK;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, eventCommandStatus);
        verify(mHeadsetClientService, timeout(QUERY_CURRENT_CALLS_TEST_WAIT_MILLIS).times(4))
                .sendBroadcast(
                intentArgument.capture(),
                anyString());

        // Verify that the new call is being registered with the inBandRing flag set.
        Assert.assertEquals(true,
                ((BluetoothHeadsetClientCall) intentArgument.getValue().getParcelableExtra(
                        BluetoothHeadsetClient.EXTRA_CALL)).isInBandRing());

        // Disable In Band Ring and verify state gets propagated.
        eventInBandRing.valueInt = 0;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, eventInBandRing);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(5)).sendBroadcast(
                intentArgument.capture(),
                anyString());
        Assert.assertEquals(0,
                intentArgument.getValue().getIntExtra(BluetoothHeadsetClient.EXTRA_IN_BAND_RING,
                        -1));
        Assert.assertEquals(false, mHeadsetClientStateMachine.getInBandRing());

    }

    /* Utility function to simulate HfpClient is connected. */
    private int setUpHfpClientConnection(int startBroadcastIndex) {
        // Trigger an incoming connection is requested
        StackEvent connStCh = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        connStCh.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_CONNECTED;
        connStCh.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, connStCh);
        ArgumentCaptor<Intent> intentArgument = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(startBroadcastIndex))
                .sendBroadcast(intentArgument.capture(), anyString());
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTING,
                intentArgument.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));
        startBroadcastIndex++;
        return startBroadcastIndex;
    }

    /* Utility function to simulate SLC connection. */
    private int setUpServiceLevelConnection(int startBroadcastIndex) {
        // Trigger SLC connection
        StackEvent slcEvent = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        slcEvent.valueInt = HeadsetClientHalConstants.CONNECTION_STATE_SLC_CONNECTED;
        slcEvent.valueInt2 = HeadsetClientHalConstants.PEER_FEAT_ECS;
        slcEvent.device = mTestDevice;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, slcEvent);
        ArgumentCaptor<Intent> intentArgument = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(startBroadcastIndex))
                .sendBroadcast(intentArgument.capture(), anyString());
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTED,
                intentArgument.getValue().getIntExtra(BluetoothProfile.EXTRA_STATE, -1));
        startBroadcastIndex++;
        return startBroadcastIndex;
    }

    /* Utility function: supported AT command should lead to native call */
    private void runSupportedVendorAtCommand(String atCommand, int vendorId) {
        // Return true for priority.
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        int expectedBroadcastIndex = 1;

        expectedBroadcastIndex = setUpHfpClientConnection(expectedBroadcastIndex);
        expectedBroadcastIndex = setUpServiceLevelConnection(expectedBroadcastIndex);

        Message msg = mHeadsetClientStateMachine.obtainMessage(
                HeadsetClientStateMachine.SEND_VENDOR_AT_COMMAND, vendorId, 0, atCommand);
        mHeadsetClientStateMachine.sendMessage(msg);

        verify(mNativeInterface, timeout(STANDARD_WAIT_MILLIS).times(1)).sendATCmd(
                Utils.getBytesFromAddress(mTestDevice.getAddress()),
                HeadsetClientHalConstants.HANDSFREECLIENT_AT_CMD_VENDOR_SPECIFIC_CMD,
                0, 0, atCommand);
    }

    /**
     *  Test: supported vendor specific command: set operation
     */
    @LargeTest
    @Test
    public void testSupportedVendorAtCommandSet() {
        int vendorId = BluetoothAssignedNumbers.APPLE;
        String atCommand = "+XAPL=ABCD-1234-0100,100";
        runSupportedVendorAtCommand(atCommand, vendorId);
    }

    /**
     *  Test: supported vendor specific command: read operation
     */
    @LargeTest
    @Test
    public void testSupportedVendorAtCommandRead() {
        int vendorId = BluetoothAssignedNumbers.APPLE;
        String atCommand = "+APLSIRI?";
        runSupportedVendorAtCommand(atCommand, vendorId);
    }

    /* utility function: unsupported vendor specific command shall be filtered. */
    public void runUnsupportedVendorAtCommand(String atCommand, int vendorId) {
        // Return true for priority.
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        int expectedBroadcastIndex = 1;

        expectedBroadcastIndex = setUpHfpClientConnection(expectedBroadcastIndex);
        expectedBroadcastIndex = setUpServiceLevelConnection(expectedBroadcastIndex);

        Message msg = mHeadsetClientStateMachine.obtainMessage(
                HeadsetClientStateMachine.SEND_VENDOR_AT_COMMAND, vendorId, 0, atCommand);
        mHeadsetClientStateMachine.sendMessage(msg);

        verify(mNativeInterface, timeout(STANDARD_WAIT_MILLIS).times(0))
                .sendATCmd(any(), anyInt(), anyInt(), anyInt(), any());
    }

    /**
     *  Test: unsupported vendor specific command shall be filtered: bad command code
     */
    @LargeTest
    @Test
    public void testUnsupportedVendorAtCommandBadCode() {
        String atCommand = "+XAAPL=ABCD-1234-0100,100";
        int vendorId = BluetoothAssignedNumbers.APPLE;
        runUnsupportedVendorAtCommand(atCommand, vendorId);
    }

    /**
     *  Test: unsupported vendor specific command shall be filtered:
     *  no back to back command
     */
    @LargeTest
    @Test
    public void testUnsupportedVendorAtCommandBackToBack() {
        String atCommand = "+XAPL=ABCD-1234-0100,100; +XAPL=ab";
        int vendorId = BluetoothAssignedNumbers.APPLE;
        runUnsupportedVendorAtCommand(atCommand, vendorId);
    }

    /* Utility test function: supported vendor specific event
     * shall lead to broadcast intent
     */
    private void runSupportedVendorEvent(int vendorId, String vendorEventCode,
            String vendorEventArgument) {
        // Setup connection state machine to be in connected state
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        int expectedBroadcastIndex = 1;
        expectedBroadcastIndex = setUpHfpClientConnection(expectedBroadcastIndex);
        expectedBroadcastIndex = setUpServiceLevelConnection(expectedBroadcastIndex);

        // Simulate a known event arrive
        String vendorEvent = vendorEventCode + vendorEventArgument;
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_UNKNOWN_EVENT);
        event.device = mTestDevice;
        event.valueString = vendorEvent;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, event);

        // Validate broadcast intent
        ArgumentCaptor<Intent> intentArgument = ArgumentCaptor.forClass(Intent.class);
        verify(mHeadsetClientService, timeout(STANDARD_WAIT_MILLIS).times(expectedBroadcastIndex))
                .sendBroadcast(intentArgument.capture(), anyString());
        Assert.assertEquals(BluetoothHeadsetClient.ACTION_VENDOR_SPECIFIC_HEADSETCLIENT_EVENT,
                intentArgument.getValue().getAction());
        Assert.assertEquals(vendorId,
                intentArgument.getValue().getIntExtra(BluetoothHeadsetClient.EXTRA_VENDOR_ID, -1));
        Assert.assertEquals(vendorEventCode,
                intentArgument.getValue().getStringExtra(
                    BluetoothHeadsetClient.EXTRA_VENDOR_EVENT_CODE));
        Assert.assertEquals(vendorEvent,
                intentArgument.getValue().getStringExtra(
                    BluetoothHeadsetClient.EXTRA_VENDOR_EVENT_FULL_ARGS));
    }

    /**
     *  Test: supported vendor specific response: response to read command
     */
    @LargeTest
    @Test
    public void testSupportedVendorEventReadResponse() {
        final int vendorId = BluetoothAssignedNumbers.APPLE;
        final String vendorResponseCode = "+XAPL=";
        final String vendorResponseArgument = "iPhone,2";
        runSupportedVendorEvent(vendorId, vendorResponseCode, vendorResponseArgument);
    }

    /**
     *  Test: supported vendor specific response: response to test command
     */
    @LargeTest
    @Test
    public void testSupportedVendorEventTestResponse() {
        final int vendorId = BluetoothAssignedNumbers.APPLE;
        final String vendorResponseCode = "+APLSIRI:";
        final String vendorResponseArgumentWithSpace = "  2";
        runSupportedVendorEvent(vendorId, vendorResponseCode, vendorResponseArgumentWithSpace);
    }

    /* Utility test function: unsupported vendor specific response shall be filtered out*/
    public void runUnsupportedVendorEvent(int vendorId, String vendorEventCode,
            String vendorEventArgument) {
        // Setup connection state machine to be in connected state
        when(mHeadsetClientService.getConnectionPolicy(any(BluetoothDevice.class))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        int expectedBroadcastIndex = 1;
        expectedBroadcastIndex = setUpHfpClientConnection(expectedBroadcastIndex);
        expectedBroadcastIndex = setUpServiceLevelConnection(expectedBroadcastIndex);

        // Simulate an unknown event arrive
        String vendorEvent = vendorEventCode + vendorEventArgument;
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_UNKNOWN_EVENT);
        event.device = mTestDevice;
        event.valueString = vendorEvent;
        mHeadsetClientStateMachine.sendMessage(StackEvent.STACK_EVENT, event);

        // Validate no broadcast intent
        verify(mHeadsetClientService, atMost(expectedBroadcastIndex - 1))
                .sendBroadcast(any(), anyString());
    }

    /**
     * Test unsupported vendor response: bad read response
     */
    @LargeTest
    @Test
    public void testUnsupportedVendorEventBadReadResponse() {
        final int vendorId = BluetoothAssignedNumbers.APPLE;
        final String vendorResponseCode = "+XAAPL=";
        final String vendorResponseArgument = "iPhone,2";
        runUnsupportedVendorEvent(vendorId, vendorResponseCode, vendorResponseArgument);
    }

    /**
     * Test unsupported vendor response: bad test response
     */
    @LargeTest
    @Test
    public void testUnsupportedVendorEventBadTestResponse() {
        final int vendorId = BluetoothAssignedNumbers.APPLE;
        final String vendorResponseCode = "+AAPLSIRI:";
        final String vendorResponseArgument = "2";
        runUnsupportedVendorEvent(vendorId, vendorResponseCode, vendorResponseArgument);
    }
}
