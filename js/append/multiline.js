(function(WEBRUBY) {
  WEBRUBY.prototype.multiline_run_source = function(src) {
    var stack = Runtime.stackSave();
    var addr = Runtime.stackAlloc(src.length);
    var ret;
    writeStringToMemory(src, addr);

    ret = _multiline_parse_run_source(this.mrb, addr, this.print_level);

    Runtime.stackRestore(stack);
    return ret;
  };
}) (WEBRUBY);
