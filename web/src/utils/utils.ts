import { smdns } from 'capacitor-community-smdns'
import { Capacitor } from '@capacitor/core'
import { getStaticIp, getServerAlias } from './storage'

let server_host: undefined | string = undefined
let static_ip: null | string = null

const listen_cb: Array<() => void> = []

getStaticIp().then((result) => {
  static_ip = result.value
  if (!static_ip) return

  listen_cb.map((cb) => cb())
})

smdns.discoverServices('_http._tcp', {
  onServiceFound: (name, host, port) => {
    console.log(name, host, port)

    if (static_ip) {
      if (server_host) return

      server_host = static_ip
      return listen_cb.map((cb) => cb())
    }

    const array = name!.split('::')

    if (array.length != 2) return

    getServerAlias().then((server_alias) => {
      if (server_alias.value?.length) {
        if (array[0] != `Clipboard-server-${server_alias.value}`) return
      } else {
        if (array[0] != 'Clipboard-server') return
      }

      if (server_host == array[1]) return

      server_host = array[1]

      listen_cb.map((cb) => cb())
    })
  }
})

export function ServerFound(cb?: () => void): boolean {
  if (Capacitor.getPlatform() == 'web') {
    return cb?.(), true
  }

  if (static_ip || server_host) return cb?.() || true

  if (cb) listen_cb.push(cb)

  return !!(static_ip || server_host)
}

export function isPlatform(name: string): boolean {
  switch (name) {
    case 'Capacitor': {
      return Capacitor.isNativePlatform()
    }
  }

  return false
}

export async function myfetch(url: string, opt?: RequestInit): Promise<Response> {
  if (!url.startsWith('/')) url = '/' + url

  if (Capacitor.getPlatform() == 'web') {
    return fetch(url, opt)
  }

  let _server_host = server_host
  let url_prefix = 'http://'

  if (!server_host) {
    if (static_ip) {
      if (/^http/.test(static_ip)) {
        url_prefix = ''
      }
      _server_host = static_ip
    } else {
      throw 'no server_host'
    }
  }

  return fetch(url_prefix + _server_host + url, opt)
}
