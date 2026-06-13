" Vim syntax file
" Language: Starch Assembly (sta)

" Prevents loading the syntax rules if already loaded
if exists("b:current_syntax")
  finish
endif

" Define Starch assembly custom groups
syntax keyword staKeyword define include section strings SFP
syntax match staNumber "\v<\d+>|\v<0x\x+>|'.'"
syntax match staComment "//.*$"
syntax match staLabel "\v:\S+"
syntax match staMacro "\v\$\S+"
syntax region staString start=/"/ skip=/\v[^\\]\\(\\\\)*"/ end=/"/
syntax region staMultiComment start="/\*" end="\*/"

" Link custom groups to Vim's standard highlight system
highlight default link staKeyword Statement
highlight default link staNumber Constant
highlight default link staComment Comment
highlight default link staLabel Label
highlight default link staMacro Macro
highlight default link staString String
highlight default link staMultiComment Comment

" Register the current syntax
let b:current_syntax = "sta"
