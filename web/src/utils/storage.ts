import { Preferences } from '@capacitor/preferences'

// export default { StaticIp }

export function setStaticIp(s: string) {
  Preferences.set({ key: 'static-ip', value: s })
}

export function getStaticIp() {
  return Preferences.get({ key: 'static-ip' })
}

export function setServerAlias(s: string) {
  Preferences.set({ key: 'server-alias', value: s })
}

export function getServerAlias() {
  return Preferences.get({ key: 'server-alias' })
}
