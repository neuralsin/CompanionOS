package com.companionos.app

import android.content.Context
import android.content.SharedPreferences
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** String preferences encrypted with a non-exportable, per-install Keystore key. */
class SecurePrefs private constructor(private val raw: SharedPreferences) : SharedPreferences by raw {
    companion object {
        private const val ALIAS = "companion.settings.v1"
        private const val PREFIX = "enc:v1:"
        fun open(context: Context, name: String): SharedPreferences =
            SecurePrefs(context.getSharedPreferences(name, Context.MODE_PRIVATE))

        @Synchronized private fun key(): SecretKey {
            val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
            (store.getKey(ALIAS, null) as? SecretKey)?.let { return it }
            return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore").apply {
                init(KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build())
            }.generateKey()
        }
        private fun encrypt(value: String): String {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, key())
            return PREFIX + Base64.encodeToString(cipher.iv + cipher.doFinal(value.toByteArray(Charsets.UTF_8)), Base64.NO_WRAP)
        }
    }

    override fun getString(key: String?, defValue: String?): String? {
        val stored = raw.getString(key, null) ?: return defValue
        if (!stored.startsWith(PREFIX)) {
            // Migrate existing installs without losing their explicitly saved values.
            raw.edit().putString(key, encrypt(stored)).apply()
            return stored
        }
        return try {
            val data = Base64.decode(stored.removePrefix(PREFIX), Base64.NO_WRAP)
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(128, data.copyOfRange(0, 12)))
            String(cipher.doFinal(data.copyOfRange(12, data.size)), Charsets.UTF_8)
        } catch (_: Exception) { defValue }
    }

    @android.annotation.SuppressLint("CommitPrefEdits") // The returned Editor preserves commit/apply through delegation.
    override fun edit(): SharedPreferences.Editor {
        val editor = raw.edit()
        return object : SharedPreferences.Editor by editor {
            override fun putString(key: String?, value: String?): SharedPreferences.Editor {
                editor.putString(key, value?.let { encrypt(it) })
                return this
            }
        }
    }
}
