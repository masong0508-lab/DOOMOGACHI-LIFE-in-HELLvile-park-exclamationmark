# the E0 additions a built-in pack may use (make -C core story-check
# compiles this tree into the build directory, never into core/gen)
@pack endx kind=side role=ending

script endx_card
  jmatn on endx_tame
  show cg 3
  [R30]
  show off
endx_tame:
  push ending E03
  push ending nope
  push mslot 5
end
