import { type ObjectPlugin, ref } from 'vue'

import * as translations from '@/../po/translations.json'

const messages: Record<string, Record<string, string>> = (translations as any).default

export const locale = ref(navigator.language)

let target_lang: string = null! as string
let target_message: Record<string, string> = null! as Record<string, string>

export const _ = function (key: string): string {
  let lang = locale.value

  if (target_lang == lang && target_message) return target_message[key] || key

  try {
    if (lang in messages) throw lang

    for (lang of navigator.languages) {
      if (lang in messages) throw lang
    }
  } catch (lang: any) {
    target_lang = lang
    target_message = messages[target_lang]

    return target_message[key] || key
  }

  return key
}

export default {
  locale,
  install: (app, options) => {
    app.config.globalProperties.$_ = _
  }
} as ObjectPlugin
