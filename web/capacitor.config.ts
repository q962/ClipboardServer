import type { CapacitorConfig } from '@capacitor/cli'

const config: CapacitorConfig = {
  appId: 'io.github.q962.ClipboardClient',
  appName: 'Remote Clipboard',
  webDir: 'dist-app',

  plugins: {
    CapacitorHttp: {
      enabled: true
    }
  },
  android: {
    buildOptions: {
      releaseType: 'APK'
    }
  }
}

export default config
