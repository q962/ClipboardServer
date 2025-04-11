<template>
  <PullToRefresh
    ref="pull_to_refresh_widget"
    :enabled="pull_to_refresh_enable"
    @scrollend="reload2"
  >
    <v-infinite-scroll
      :key="componentKey"
      ref="infinite_scroll_widget"
      :items="items"
      :onLoad="loadnext"
    >
      <template v-for="(item, index) in items" :key="item">
        <ClipDataItem :item="item" :item_id="item_ids[index]" />
      </template>
    </v-infinite-scroll>
  </PullToRefresh>
</template>

<script setup lang="ts">
import { ref, shallowRef } from 'vue'
import { myfetch } from '../utils/utils'
import ClipDataItem from './ClipDataItem.vue'
import PullToRefresh from './PullToRefresh.vue'

const pull_to_refresh_widget = ref()
const infinite_scroll_widget = ref()

const pull_to_refresh_enable = () => {
  let element = infinite_scroll_widget.value

  return element.$el.scrollTop == 0
}

const componentKey = ref(0)

async function get_data(limit: number, count: number) {
  try {
    let params = new URLSearchParams([
      ['limit', limit.toString()],
      ['count', count.toString()]
    ])
    let response = await myfetch('/api/get_clips?' + params.toString())
    if (response.status == 204) return
    if (response.status != 200) return EventBus.emit('fetch-failed')
    let json_size = Number(response.headers.get('X-Json-Size'))
    let buffer2 = await response.arrayBuffer()
    let json_buffer = buffer2.slice(0, json_size)
    let image_buffers = buffer2.slice(json_size)
    let image_offset = 0
    let json_str = new TextDecoder().decode(json_buffer)
    let jsonResult = JSON.parse(json_str)
    let clip_datas = jsonResult[0]
    for (let i2 = 0; i2 < clip_datas.length; i2++) {
      let item = clip_datas[i2]
      if (typeof item == 'number') {
        let image_buffer = image_buffers.slice(image_offset, image_offset + item)
        image_offset += item
        clip_datas[i2] = new Uint8Array(image_buffer)
      }
    }
    return {
      datas: clip_datas,
      ids: jsonResult[1]
    }
  } catch (error) {
    EventBus.emit('fetch-failed')
  }
}

const items = shallowRef([] as Array<any>)
const item_ids = shallowRef([] as Array<any>)

async function loadnext(opt: any) {
  let state = 'ok'
  const res = await get_data(items.value.length, 30)
  if (res) {
    items.value.push(...res.datas)
    item_ids.value.push(...res.ids)
  } else if (items.value.length == 0) {
    state = 'empty'
  } else {
    state = 'error'
  }
  pull_to_refresh_widget?.value?.done()
  opt.done(state)
}
function reload2() {
  componentKey.value += 1
  items.value.length = 0
  item_ids.value.length = 0
}
</script>

<style lang="scss">
.v-infinite-scroll {
  height: 100%;
  padding: 0 12px;
}
</style>
