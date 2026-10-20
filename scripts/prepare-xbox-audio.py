#!/usr/bin/env python3
"""Stage native 48 kHz Xbox Ogg assets without modifying the source recordings."""
import argparse,json,subprocess,hashlib,tempfile,shutil,shlex
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=root/'bin/Neverball')
a=p.parse_args();cache=root/'build/audio-48000';cache.mkdir(parents=True,exist_ok=True)
# Build Xiph's BSD-licensed reference encoder from the pinned Vorbis source.
# The example expects stereo PCM; mono effects are duplicated to both channels.
example=(root/'build/deps/vorbis/examples/encoder_example.c').read_text()
example=example.replace('vorbis_encode_init_vbr(&vi,2,44100,0.1)',
                        'vorbis_encode_init_vbr(&vi,2,48000,0.6)')
encoder_source=cache/'encoder.c';encoder=cache/'encoder'
if not encoder.exists() or not encoder_source.exists() or encoder_source.read_text()!=example:
    encoder_source.write_text(example)
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','vorbisenc'],text=True))
    subprocess.run(['cc',str(encoder_source),*flags,'-lm','-o',str(encoder)],check=True)
records={}
for source in sorted((root/'data').rglob('*.ogg')):
    relative=source.relative_to(root/'data');target=cache/relative
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    cache_key=digest+'-48000-stereo-q6'
    stamp=target.with_suffix('.source-sha256')
    if not target.exists() or not stamp.exists() or stamp.read_text()!=cache_key:
        target.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryFile() as pcm:
            subprocess.run(['ffmpeg','-v','error','-i',str(source),'-ar','48000','-ac','2',
                            '-bitexact','-f','wav','-c:a','pcm_s16le','-'],stdout=pcm,check=True)
            pcm.seek(0)
            with target.open('wb') as encoded:
                subprocess.run([str(encoder)],stdin=pcm,stdout=encoded,stderr=subprocess.DEVNULL,check=True)
        stamp.write_text(cache_key)
    output=a.output/relative;output.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(target,output)
    records[relative.as_posix()]={'source_sha256':digest,'output_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'rate':48000,'channels':2,'encoder':'Xiph Vorbis 1.3.7 q0.6'}
(root/'build/audio-48000-manifest.json').write_text(json.dumps(records,indent=2)+'\n')
print('Staged',len(records),'native 48 kHz Ogg files')
