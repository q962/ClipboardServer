<template>
  <v-card :title="$_('Load Failed!')">
    <template #text>
      {{ $_('Unable to communicate with the server.') }}<br /><br />
      {{ $_('Possible causes and solutions:') }}<br />
      <ol>
        <li>
          {{ $_('The system does not support mdns and cannot automatically obtain your host ip')
          }}<br />
          {{ $_('Try setting a static ip for your computer') }}<br />
          <v-text-field
            v-model="static_ip"
            hide-details="auto"
            :label="$_('IP:Port')"
          ></v-text-field>
        </li>
        <li>
          {{ $_('Set Server Alias') }}<br />
          <v-text-field
            v-model="server_alias"
            hide-details="auto"
            :label="$_('Alias')"
          ></v-text-field>
        </li>
        <li>
          {{ $_('Network failure') }}<br />
          {{ $_('Try reload') }}
        </li>
        <li>
          {{ $_('Not connected to wifi') }}<br />
          {{ $_('Only works in LAN') }}
        </li>
        <li>{{ $_('Server program not installed') }}</li>
      </ol>
      <br />
      <hr height="1px" />
      <br />

      {{ $_('Just disconnect from wifi and open the app to display the current page') }}
    </template>
    <v-card-actions>
      <v-btn @click="reload()">{{ $_('Reload') }}</v-btn>
    </v-card-actions>
  </v-card>
</template>

<script setup lang="ts">
import { ref, watch, onMounted } from 'vue'
import { setStaticIp, getStaticIp, setServerAlias, getServerAlias } from '@/utils/storage'

const static_ip = ref<string>('')
const server_alias = ref<string>('')

watch(static_ip, (s) => {
  setStaticIp(s as any)
})

watch(server_alias, (s) => {
  setServerAlias(s as any)
})

function reload() {
  window.location.reload()
}

onMounted(() => {
  getStaticIp().then((res) => {
    static_ip.value = res.value ?? ''
  })
  getServerAlias().then((res) => {
    server_alias.value = res.value ?? ''
  })
})
</script>

<style scoped lang="scss">
ol {
  margin-left: 1em;
}
li {
  margin-top: 15px;
}
</style>
