package com.companionos.app

object BotProtocol {
    const val PAGE_COUNT = 20
    fun isPrivateIpv4(ip: String): Boolean {
        val parts = ip.split('.')
        if (parts.size != 4 || parts.any { it.isEmpty() || it.any { c -> c !in '0'..'9' } }) return false
        val p = parts.map { it.toIntOrNull() ?: return false }
        if (p.any { it !in 0..255 } || p[3] == 0 || p[3] == 255) return false
        return p[0] == 10 || (p[0] == 192 && p[1] == 168) || (p[0] == 172 && p[1] in 16..31)
    }
    fun isAllowedCommand(command: String): Boolean = when {
        command.startsWith("PAGE:") -> (command.substringAfter(':').toIntOrNull() ?: -1) in 0 until PAGE_COUNT
        command.startsWith("THEME:") -> (command.substringAfter(':').toIntOrNull() ?: -1) in 0..2
        command.startsWith("THOUGHT:") -> command.substringAfter(':').toByteArray(Charsets.UTF_8).size in 1..79 && !command.contains('\n')
        else -> command in setOf("BTN:DISMISS", "BTN:LEFT", "BTN:RIGHT", "BTN:UP", "BTN:DOWN", "BTN:SELECT", "BTN:SELECT_LONG", "BTN:HOME",
            "EMOTION:AUTO", "EMOTION:NEUTRAL", "EMOTION:HAPPY", "EMOTION:SAD", "EMOTION:EXCITED", "EMOTION:LOVE",
            "EMOTION:SLEEPY", "EMOTION:ANGRY", "EMOTION:SURPRISED", "CMD:PLAY_PAUSE", "CMD:NEXT", "CMD:PREV",
            "CMD:POMO:START", "CMD:POMO:PAUSE", "CMD:POMO:RESET", "CUSTEYE:OFF", "CUSTEYE:TOGGLE:1")
    }
}
