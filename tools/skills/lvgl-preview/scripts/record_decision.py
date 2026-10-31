#!/usr/bin/env python3
"""Optional explicit human feedback; never a prerequisite for native completion."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from preview import inside,append_ledger,PreviewError

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root',required=True)
    parser.add_argument('--decision',choices=('OK','NO-GO'),required=True)
    args=parser.parse_args();root=Path(args.project_root).expanduser().resolve();directory=root/'.lvgl-preview';path=directory/'state.json'
    try:
        state=json.loads(path.read_text())
        if state.get('user_feedback') in ('OK','NO-GO'):
            raise PreviewError('explicit feedback already recorded for this run')
        rollback='not requested'
        if args.decision=='NO-GO':
            for name in state['touched_files']:
                source=inside(root,root/state['backup_dir']/name);target=inside(root,root/name)
                original=source.read_bytes();target.write_bytes(original)
                if hashlib.sha256(target.read_bytes()).digest()!=hashlib.sha256(original).digest():
                    raise PreviewError(f'rollback verification failed: {name}')
            rollback='exact backup verified';state['completion_status']='restored_after_feedback'
        state.pop('user_decision',None);state['user_feedback']=args.decision;state['rollback']=rollback
        path.write_text(json.dumps(state,indent=2)+'\n')
        append_ledger(directory,{'id':state['run_id'],'user_feedback':args.decision,'rollback':rollback,'reason':'explicit user feedback; independent of render evaluation'})
        print(json.dumps({'user_feedback':args.decision,'rollback':rollback}))
        return 0
    except (OSError,ValueError,KeyError,PreviewError)as exc:
        print('FAIL '+str(exc));return 1
if __name__=='__main__':raise SystemExit(main())
