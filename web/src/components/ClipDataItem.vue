<template>
  <div class="clip-data-item">
    <template v-if="typeof item == 'string'">
      <textarea
        @click.stop="toggle_rows_v2()"
        :class="{ unscroll: has_more_rows() && !is_show_all_rows }"
        class="float"
        :rows="rows"
        readonly
        :value="item"
      ></textarea>
      <div class="btn-box">
        <v-btn class="copy-btn unselectable" variant="plain" @click="set_clip()">
          {{ $_('Copy') }}
        </v-btn>
        <v-icon
          class="expand-icon"
          v-show="text_rows > rows || is_show_all_rows"
          :icon="is_show_all_rows ? 'mdi-chevron-up' : 'mdi-chevron-down'"
          size="large"
          @click="toggle_rows()"
        ></v-icon>
      </div>
    </template>
    <template v-else>
      <img ref="image_elem" @click="image_click" class="small_img" :src="image_url" />
      <v-btn class="save-btn unselectable" variant="plain" @click="save_img">
        {{ $_('Save') }}
      </v-btn>
    </template>
  </div>

  <warning class="warning" ref="warningRef"></warning>
</template>

<script setup lang="ts">
import { ref, defineProps, computed, onMounted, getCurrentInstance } from 'vue'
import warning from './warning.vue'
import { Clipboard } from '@capacitor/clipboard'
import { Capacitor } from '@capacitor/core'
import { Media } from '@capacitor-community/media'

const instance = getCurrentInstance()

const props = defineProps(['item', 'item_id']) as {
  item: string | number | Uint8Array
  item_id: number
}

const warningRef = ref()

const image_elem = ref<HTMLImageElement>()

let imgBlob = undefined! as Blob
let imgBlob_url: string = ''
let imgType: string

const image_url = ref()

onMounted(async () => {
  if (props.item instanceof Uint8Array) {
    imgType = image_type(props.item)
    imgBlob = new File([new Blob([props.item as Uint8Array])], 'a.' + imgType, {
      type: 'image/' + imgType
    }) as Blob

    imgBlob_url = window.URL.createObjectURL(imgBlob)

    image_url.value = await blob_to_base64(imgBlob)
  }
})

function image_click() {
  instance?.proxy?.$viewerApi({
    images: [image_url.value],
    options: {
      navbar: false
    }
  })
}

const text_rows = computed(() => {
  return (props.item as string).split(/\r\n|\r|\n/).length
})

let is_show_all_rows = ref(false)
const rows = computed(() => {
  if (text_rows.value > 3) {
    if (is_show_all_rows.value) return text_rows.value > 10 ? 10 : text_rows.value
    else return 3
  }
  return text_rows.value
})

const has_more_rows = () => {
  if (text_rows.value > 3) {
    return true
  }
  return false
}

function toggle_rows_v2() {
  if (!has_more_rows()) return

  if (!is_show_all_rows.value) toggle_rows()
}

function toggle_rows() {
  is_show_all_rows.value = !is_show_all_rows.value
}

function set_clip() {
  Clipboard.write({
    string: props.item as string
  }).catch(() => {
    warningRef.value.show(_('Failed to set clipboard content: No permission'))
  })
}

async function get_album() {
  const album_name = _('Remote Clipboard Picture')
  const { albums } = await Media.getAlbums()

  let album = albums.find((a) => a.name === album_name)
  if (!album) {
    await Media.createAlbum({ name: album_name })

    const { albums } = await Media.getAlbums()
    album = albums.find((a) => a.name === album_name)
  }

  return album
}

async function blob_to_base64(data: Blob) {
  return (await new Promise((r) => {
    const reader = new FileReader()
    reader.onload = () => r(reader.result as string)
    reader.readAsDataURL(data)
  })) as string
}

async function save_img() {
  if (Capacitor.isNativePlatform()) {
    const album = await get_album()
    await Media.savePhoto({ path: image_url.value, albumIdentifier: album?.identifier })
  } else {
    let a = document.createElement('a')
    a.download = imgBlob_url.split('/')[3] + '.' + imgType
    a.href = imgBlob_url
    a.click()
  }
}

function image_type(array: Uint8Array) {
  const view = new DataView(array.buffer)

  if (view.getUint32(0) === 0x89504e47) return 'png'
  if (view.getUint16(0) === 0xffd8) return 'jpeg'
  if (view.getUint32(0) === 0x47494638) return 'gif'
  if (view.getUint32(0) === 0x52494646) return 'webp'
  if (view.getUint16(0) === 0x424d) return '/bmp'
  if (view.getUint32(0) === 0x49492a00) return 'tiff' // TIFF (little endian)
  if (view.getUint32(0) === 0x4d4d002a) return 'tiff' // TIFF (big endian)

  return ''
}
</script>

<style>
.viewer-prev,
.viewer-next {
  display: none;
}
</style>

<style scoped lang="scss">
.clip-data-item {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  border-radius: 5px;
  background: var(--color-background-soft);
  margin: 5px 0;
  padding: 10px;
  column-gap: 10px;
}

.small_img {
  max-width: 100%;
  max-height: 100px;
}

.copy-btn,
.save-btn {
  height: 100%;
  background: var(--color-background-mute-1);
}

.btn-box {
  display: grid;
  grid-template-columns: 100%;
  grid-template-rows: minmax(0, 1fr) auto;

  .expand-icon {
    margin-top: 5px;
    place-self: center;
  }
}

textarea {
  resize: unset;

  outline: unset;
  cursor: default;
  color: var(--color-text);
  text-overflow: ellipsis;
  white-space: pre;
  scrollbar-width: none;

  &.unscroll {
    overflow-y: hidden;
  }

  &::-webkit-scrollbar {
    display: none;
  }
}
</style>
