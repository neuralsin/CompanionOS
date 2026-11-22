package com.companionos.app

import org.junit.Assert.*
import org.junit.Test

class BotProtocolTest {
    @Test fun privateTargetsOnly() {
        for (ip in listOf("192.168.1.2", "10.0.0.1", "172.16.0.2", "172.31.255.254")) assertTrue(ip, BotProtocol.isPrivateIpv4(ip))
        for (ip in listOf("", "localhost", "127.0.0.1", "8.8.8.8", "172.32.0.1", "192.168.1.255", "192.168.1.300", "10.0.0.-1", "10.0.0.1:80", "10.0.0.1/path")) assertFalse(ip, BotProtocol.isPrivateIpv4(ip))
    }
    @Test fun allTilesAndBoundaries() {
        for (page in 0..19) assertTrue(BotProtocol.isAllowedCommand("PAGE:$page"))
        for (value in listOf("PAGE:20", "PAGE:-1", "PAGE:abc", "THEME:3", "THEME:-1")) assertFalse(value, BotProtocol.isAllowedCommand(value))
        assertTrue(BotProtocol.isAllowedCommand("BTN:DISMISS"))
    }
    @Test fun actionsMatchFirmware() {
        for (value in listOf("EMOTION:AUTO", "THEME:2", "CMD:PREV", "CMD:NEXT", "CMD:PLAY_PAUSE", "BTN:SELECT_LONG", "CMD:POMO:START")) assertTrue(value, BotProtocol.isAllowedCommand(value))
        for (value in listOf("SPOTIFY:NEXT", "BTN:LONG_SELECT", "EMOTION:UNKNOWN", "SPOTIFY_AUTH:secret", "DRHACK:DEAUTH")) assertFalse(value, BotProtocol.isAllowedCommand(value))
    }
    @Test fun boundedThoughts() {
        assertTrue(BotProtocol.isAllowedCommand("THOUGHT:"+"a".repeat(79)))
        assertFalse(BotProtocol.isAllowedCommand("THOUGHT:"+"a".repeat(80)))
        assertFalse(BotProtocol.isAllowedCommand("THOUGHT:"+"\u2764".repeat(27)))
        assertFalse(BotProtocol.isAllowedCommand("THOUGHT:a\nPAGE:1"))
    }
}
