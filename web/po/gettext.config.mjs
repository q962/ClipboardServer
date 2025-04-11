export default {
  input: {
    path: './src',
    parserOptions: {
      mapping: {
        simple: '_'
      }
    }
  },
  output: {
    path: './po',
    locales: ['en','zh-CN'],
    splitJson: false
  }
}
