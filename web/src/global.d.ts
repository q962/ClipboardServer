import bus from '@/utils/event.ts'
import { _ as t } from '@/utils/i18n'

declare global {
  const EventBus = bus
  const _: typeof t
}
