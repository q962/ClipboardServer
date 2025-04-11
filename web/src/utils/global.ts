import bus from '@/utils/event'
import { _ } from '@/utils/i18n'

const G = globalThis as any

G.EventBus = bus
G._ = _
