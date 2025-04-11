<template>
  <header></header>

  <main>
    <template v-if="hasServer">
      <clip-data-list />
      <Push />
    </template>
    <template v-else>
      <div class="centent-box">
        <v-skeleton-loader v-show="load_state == 0" type="paragraph"></v-skeleton-loader>
        <ServerError v-show="load_state == 1" />
      </div>
    </template>
  </main>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'

import ClipDataList from './components/ClipDataList.vue'
import Push from './components/Push.vue'
import ServerError from './components/ServerError.vue'
import { ServerFound, myfetch } from '@/utils/utils'

const hasServer = ref(false)
const load_state = ref(0)

EventBus.on('fetch-failed', () => {
  load_state.value = 1
  hasServer.value = false
})

ServerFound(async () => {
  try {
    let params = new URLSearchParams([
      ['limit', '0'],
      ['count', '0']
    ])
    let response = await myfetch('/api/get_clips?' + params.toString())
    if (response.status == 204) {
      hasServer.value = true
      return
    }
  } catch (error) {}

  EventBus.emit('fetch-failed')
})

setTimeout(() => {
  if (ServerFound()) {
    return
  }

  load_state.value = 1
}, 5000)
</script>

<style>
#app {
  padding: 0;
}
html,
body {
  overflow: hidden; /* 禁止滚动 */
}
</style>

<style scoped lang="scss">
main {
  display: flex;
  flex-direction: column;
}

header {
  line-height: 1.5;
}

.logo {
  display: block;
  margin: 0 auto 2rem;
}

@media (min-width: 1024px) {
  header {
    display: flex;
    place-items: center;
    padding-right: calc(var(--section-gap) / 2);
  }

  kdd .logo {
    margin: 0 2rem 0 0;
  }

  header .wrapper {
    display: flex;
    place-items: flex-start;
    flex-wrap: wrap;
  }
}

.centent-box {
  display: flex;
  justify-content: center;
  padding: 2em;

  > * {
    width: 100%;
    max-width: 32em;
  }
}
.clip-data-list {
  height: 100%;
}
</style>
