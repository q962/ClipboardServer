<template>
  <div
    ref="selfWidget"
    class="pull-to-refresh"
    @touchstart="touchstart($event)"
    @touchend="touchend()"
    @touchmove="touchmove($event)"
  >
    <div ref="loadingWidget" class="loading">
      <slot name="loading">
        <div class="loading default">{{ $_('Reload') }}</div>
      </slot>
    </div>
    <div class="content" ref="content_elem">
      <slot></slot>
    </div>
  </div>
</template>

<script setup lang="ts">
import {
  ref,
  onMounted,
  defineEmits,
  defineProps,
  type PropType,
  defineExpose,
  useTemplateRef
} from 'vue'

const content_elem = ref<HTMLElement>()

const props = defineProps({
  enabled: {
    type: Function as PropType<() => boolean>,
    default: () => {
      return false
    }
  },
  loading_height: {
    type: [String, Number] as PropType<'auto' | 'unlimit' | number>,
    default: 'auto'
  }
})
const emit = defineEmits(['scrolling', 'scrollend', 'scrollcancel'])

let pull_to_start = false
let waiting_done = false
let start_y = 0
let end_y = 0
let offset_y = 0
let cancel = false

const loadingWidget = ref<HTMLElement>()
const selfWidget = ref<HTMLElement>()

onMounted(() => {
  selfWidget.value!.style.top = -loadingWidget.value!.offsetHeight + 'px'
})

export type PullToRefreshDoneCallback = () => void

function done() {
  if (!waiting_done) return

  waiting_done = false

  selfWidget.value!.style.top = -loadingWidget.value!.offsetHeight + 'px'
}

function touchstart(event: TouchEvent) {
  if (!props.enabled()) return

  let start_target_elem = event.target as HTMLElement

  let content_elem_first_child = content_elem.value?.children[0]
  if (content_elem_first_child != start_target_elem && start_target_elem.scrollTop != 0)
    return (cancel = true)

  let start_touch = event.touches[0]

  start_y = start_touch.screenY
}

function step(n: number) {
  let max_height = 0
  if (n <= 0) n = 0

  switch (props.loading_height) {
    case 'unlimit': {
      max_height = 0
      break
    }
    case 'auto': {
      max_height = loadingWidget.value!.offsetHeight

      if (Math.abs(n) > max_height) n = n > 0 ? max_height : -max_height
      break
    }
    default: {
      max_height = Number(props.loading_height)

      if (Math.abs(n) > max_height) n = n > 0 ? max_height : -max_height
      break
    }
  }

  emit('scrolling', n, max_height, Math.abs(n) / (max_height ?? 1))

  selfWidget.value!.style.top = -loadingWidget.value!.offsetHeight + n + 'px'
}

function touchmove(event: TouchEvent) {
  let touch = event.touches[0]

  if (!props.enabled() || cancel) return

  if (waiting_done) return

  end_y = touch.screenY

  if (pull_to_start || end_y > start_y) {
    pull_to_start = true
    event.preventDefault()
  } else {
    return
  }

  offset_y = end_y - start_y

  step(offset_y)
}

function scrollcancel() {
  selfWidget.value!.style.top = -loadingWidget.value!.offsetHeight + 'px'

  emit('scrollcancel')
}

function touchend() {
  if (cancel) return (cancel = false)

  if (start_y == -1) return

  if (!props.enabled()) return

  if (waiting_done) return

  let distance = end_y - start_y
  start_y = -1
  end_y = -1

  if (distance < 0) {
    pull_to_start = false
    return
  }

  let max_height = 0

  switch (props.loading_height) {
    case 'unlimit': {
      emit('scrollend', done)
      break
    }
    case 'auto': {
      max_height = loadingWidget.value!.offsetHeight

      if (distance >= max_height) {
        waiting_done = true
        emit('scrollend', done)
      } else {
        scrollcancel()
      }

      break
    }
    default: {
      max_height = Number(props.loading_height)

      if (distance >= max_height) {
        waiting_done = true
        emit('scrollend', done)
      } else {
        scrollcancel()
      }
      break
    }
  }

  pull_to_start = false
}

defineExpose({
  done
})
</script>

<style lang="scss" scope>
.pull-to-refresh {
  position: relative;
  transition: top 0.3s ease-out;
  height: 100%;

  .content {
    height: 100%;
  }

  .loading.default {
    padding-top: 1em;
    text-align: center;
  }
}
</style>
