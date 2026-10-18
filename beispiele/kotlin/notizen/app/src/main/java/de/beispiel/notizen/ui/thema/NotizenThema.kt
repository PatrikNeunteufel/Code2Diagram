package de.beispiel.notizen.ui.thema

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val Hell = lightColorScheme(primary = Color(0xFF3D5AFE), secondary = Color(0xFF00BFA5))
private val Dunkel = darkColorScheme(primary = Color(0xFF8C9EFF), secondary = Color(0xFF64FFDA))

@Composable
fun NotizenThema(dunkel: Boolean = isSystemInDarkTheme(), inhalt: @Composable () -> Unit) {
    MaterialTheme(colorScheme = if (dunkel) Dunkel else Hell, content = inhalt)
}
