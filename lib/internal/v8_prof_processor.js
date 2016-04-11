const scriptFiles = [
  'internal/v8_prof_polyfill',
  v8_parent_path + '/tools/splaytree',
  v8_parent_path + '/tools/codemap',
  v8_parent_path + '/tools/csvparser',
  v8_parent_path + '/tools/consarray',
  v8_parent_path + '/tools/profile',
  v8_parent_path + '/tools/profile_view',
  v8_parent_path + '/tools/logreader',
  v8_parent_path + '/tools/tickprocessor',
  v8_parent_path + '/tools/SourceMap',
  v8_parent_path + '/tools/tickprocessor-driver'
];
var script = '';

scriptFiles.forEach(function(s) {
  script += process.binding('natives')[s] + '\n';
});

var tickArguments = [];
if (process.platform === 'darwin') {
  const nm = 'foo() { nm "$@" | (c++filt -p -i || cat) }; foo $@';
  tickArguments.push('--mac', '--nm=' + nm);
} else if (process.platform === 'win32') {
  tickArguments.push('--windows');
}
tickArguments.push.apply(tickArguments, process.argv.slice(1));
script = `(function() {
  arguments = ${JSON.stringify(tickArguments)};
  ${script}
})()`;
eval(script);
