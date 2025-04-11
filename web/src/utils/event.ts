export type EventCB = (value?: any) => void

class EventBus {
  events: Record<string, EventCB[] | undefined> = {}
  on(name: string, cb: EventCB): void {
    ;(this.events[name] = this.events[name] || []).push(cb)
  }

  emit(name: string, value?: any): void {
    this.events[name]?.map((cb) => {
      cb(value)
    })
  }
}

export default new EventBus()
