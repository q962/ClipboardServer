import { _ } from '@/utils/i18n'

export {}

declare module 'vue' {
  export interface ComponentCustomProperties {
    $_: typeof _
  }
}
