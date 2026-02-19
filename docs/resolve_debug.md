# Resolve OFX Debug

If Resolve shows `failed to load` after replacing the plugin binary, clear the OFX plugin cache and restart Resolve.

## OFXPluginCache.xml locations

- Windows: `C:\Users\<YourUser>\AppData\Roaming\Blackmagic Design\DaVinci Resolve\OFXPluginCache.xml`
- macOS: `~/Library/Application Support/Blackmagic Design/DaVinci Resolve/OFXPluginCache.xml`

## Quick recovery steps

1. Close DaVinci Resolve.
2. Replace/update the `.ofx` plugin file.
3. Delete `OFXPluginCache.xml`.
4. Start Resolve again and check OFX list.
