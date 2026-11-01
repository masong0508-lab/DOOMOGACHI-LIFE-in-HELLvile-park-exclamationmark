# e0_user.dsc - a SFW user pack for the E0 contract tests (owner: E0;
# core/tests/test_story_vm.c, `make -C core story-check` rebuilds it with
# `textpack.py --user` and compares it with e0_user.pack byte for byte).
# It shows what a content pack's scene script may do (pack.h PACK_OPS_*):
# talk, wait, branch on its own vars and the Mature level, show its art.
# Script ids are TXT_ID(112, k): k = 1 e0u_guard, 2 e0u_show, 3 e0u_call,
# 4 e0u_sub (index 0 is the empty script every pack starts with).
@pack e0user kind=side

string E0U_HELLO "A tasteful pan to the window. @P blushes."

script e0u_guard
  setv v2 0
  jmat on full
  jmat censored mosaic
  setv v2 1
  jmatn pack nopack
  end
nopack:
  setv v3 7
  end
mosaic:
  setv v2 2
  end
full:
  setv v2 3
end

script e0u_show
  say nar "Meanwhile, a curtain does its job."
  show cg 7
  [R10]
  show off
  setv v4 1
end

script e0u_call
  call e0u_sub
  addv v5 1
end

script e0u_sub
  setv v5 40
  ret
end
