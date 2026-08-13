#!/usr/bin/env python3
"""Mirror res/GL -> res/Vulkan with Vulkan-specific rewrites. Idempotent.

Transforms per GL->VK mapping rules:
  - #version 330/430 core -> #version 450 core
  - UniformBlock: layout(binding = 0) uniform UniformBlock -> layout(set=0, binding=0)
  - samplers: 1st..Nth declaration -> layout(set=0, binding=1..N) (block owns binding 0)
  - gl_Layer assignment in geometry shaders -> commented out (Task 10 handles layered render)
  - .vs/.fs/.gs -> .vert/.frag/.geom
  - Common/UniformBlock.glsl copied+rewritten (not a compile target: no #version)
"""
import os
import re
import shutil
import sys

EXT_MAP = {'.vs': '.vert', '.fs': '.frag', '.gs': '.geom'}
SHADER_EXTS = ('.vert', '.frag', '.vs', '.fs', '.gs', '.geom')
MAX_SAMPLER_BINDINGS = 15

_UNIFORM_BLOCK_RE = re.compile(
    r'layout\s*\(\s*binding\s*=\s*0\s*\)\s*(uniform\s+UniformBlock\b)'
)
_SAMPLER_RE = re.compile(r'uniform\s+sampler\w+\s+\w+\s*;')
_SAMPLER_DECL_RE = re.compile(r'^uniform\s+(sampler\w+)\s+(\w+)\s*;')
_GL_LAYER_RE = re.compile(r'^\s*(gl_Layer\s*=\s*[^;]+;)', re.MULTILINE)
_VERSION_RE = re.compile(r'#version\s+(?:330|430)\s+core\b')


def convert_src(text, ext):
    text = _VERSION_RE.sub('#version 450 core', text)
    if 'uniform UniformBlock' in text:
        text = _UNIFORM_BLOCK_RE.sub(r'layout(set=0, binding=0) \1', text)
    if _SAMPLER_RE.search(text):
        text = _rewrite_samplers(text)
    if ext == '.geom':
        text = _GL_LAYER_RE.sub(r'// \1', text)
    return text


def _rewrite_samplers(text):
    """Rewrite sampler declarations to layout(set=0, binding=N) in declaration order."""
    samplers = _SAMPLER_RE.findall(text)
    if len(samplers) > MAX_SAMPLER_BINDINGS:
        raise RuntimeError(
            'too many samplers (%d > %d)' % (len(samplers), MAX_SAMPLER_BINDINGS)
        )
    out = []
    pos = 0
    for idx, decl in enumerate(samplers):
        match = _SAMPLER_RE.search(text, pos)
        m = _SAMPLER_DECL_RE.match(decl)
        binding = idx + 1
        out.append(text[pos:match.start()])
        out.append('layout(set=0, binding=%d) uniform %s %s;' % (binding, m.group(1), m.group(2)))
        pos = match.end()
    out.append(text[pos:])
    return ''.join(out)


def main():
    src_root, dst_root = sys.argv[1], sys.argv[2]
    map_lines = []
    shader_count = 0
    for root, _, files in os.walk(src_root):
        for fn in sorted(files):
            src = os.path.join(root, fn)
            if not fn.endswith(SHADER_EXTS):
                continue
            rel = os.path.relpath(src, src_root)
            base, old_ext = os.path.splitext(rel)
            new_ext = EXT_MAP.get(old_ext, old_ext)
            out_rel = base + new_ext
            dst = os.path.join(dst_root, out_rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(src) as f:
                text = f.read()
            new_text = convert_src(text, new_ext)
            with open(dst, 'w') as f:
                f.write(new_text)
            shader_count += 1
            print('%s -> %s' % (rel, out_rel))
            bindings = _SAMPLER_RE.findall(new_text)
            for idx, decl in enumerate(bindings):
                m = _SAMPLER_DECL_RE.match(decl)
                map_lines.append('%s\t%s\t%d' % (out_rel, m.group(2), idx + 1))

    os.makedirs(os.path.join(dst_root, 'Common'), exist_ok=True)
    shutil.copyfile(os.path.join(src_root, 'Common', 'UniformBlock.glsl'),
                    os.path.join(dst_root, 'Common', 'UniformBlock.glsl'))
    common = os.path.join(dst_root, 'Common', 'UniformBlock.glsl')
    with open(common) as f:
        text = f.read()
    text = text.replace('layout(binding = 0) uniform UniformBlock {',
                        'layout(set=0, binding=0) uniform UniformBlock {')
    with open(common, 'w') as f:
        f.write(text)

    if map_lines:
        with open(os.path.join(dst_root, 'sampler_binding_map.txt'), 'w') as f:
            f.write('# shader\tsampler\tbinding (set=0; binding=unit+1)\n')
            f.write('\n'.join(sorted(map_lines)))
            f.write('\n')

    print('generated %d shaders into %s' % (shader_count, dst_root))


if __name__ == '__main__':
    main()
