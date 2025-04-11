import { fileURLToPath, URL } from 'node:url'

import { loadEnv, defineConfig, PluginOption, UserConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

const injectString = (
  targetFile_endsWith: string,
  insertString: string | string[],
  after: boolean = true
): PluginOption => {
  if (Array.isArray(insertString)) insertString = insertString.join('\n')

  return {
    name: 'inject-string',
    transform(code, id) {
      const filePath = id.replaceAll('\\', '/')
      if (filePath.endsWith(targetFile_endsWith.replaceAll('\\', '/'))) {
        return {
          code: after ? code + ';\n' + insertString : insertString + ';\n' + code
        }
      }

      return null
    }
  }
}

// https://vitejs.dev/config/
export default defineConfig(({ command, mode, isSsrBuild, isPreview }) => {
  void command, mode, isSsrBuild, isPreview
  const env = loadEnv(mode, process.cwd(), '')

  const modes: {
    [key: string]: () => UserConfig
  } = {
    app: () => ({
      base: '/',
      build: {
        outDir: 'dist-app',
        minify: !env.DEBUG,
        rollupOptions: {
          treeshake: false
        }
      }
    })
  }

  return Object.assign(
    {
      base: '/web',

      build: {
        minify: !env.DEBUG,
        rollupOptions: {
          treeshake: false
        }
      },
      plugins: [
        vue(),
        vueDevTools(),
        env.DEBUG
          ? injectString('/main.ts', ["import eruda from 'eruda'", 'eruda.init()'], false)
          : null
      ],
      resolve: {
        alias: {
          '@': fileURLToPath(new URL('./src', import.meta.url))
        }
      },
      server: {
        proxy: {
          '/api/get_clips': env.CLIPBOARD_SERVER,
          '/api/push': env.CLIPBOARD_SERVER
        }
      }
    },
    modes[mode]?.()
  )
})
