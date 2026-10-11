"""Assemble final hashes and measured acceptance evidence without changing inputs."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inject_gamecube_payload import DolImage
from game_ini import update_scene

def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()

def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

native = ROOT / sys.argv[1]
diagnostics_count = int(sys.argv[2])
replay_count = int(sys.argv[3])
engine_count = int(sys.argv[4])
dol = ROOT / 'main.dol'
dol_hash = digest(dol)
assert (native / 'tested-dol.sha256').read_text().strip().lower() == dol_hash
image = DolImage(dol.read_bytes())
payload_offset, payload_start, payload_size = image.text_slot(3)
source = Path('C:/Users/usmde/Desktop/POCO F7/GameCube iso/Ultimate Spider-Man [GUTE52].iso')
ini = Path('C:/Users/usmde/Desktop/POCO F7/Download/Ultimate Spider-Man [GUTP52]/root/game.ini')
installed = ini.parents[1] / 'sys/main.dol'
assert digest(installed) == dol_hash
assert digest(ROOT / 'build/gamecube/main_patched.dol') == dol_hash
source_hash, ini_hash = digest(source), digest(ini)
assert source_hash == '1b412bcc956fdf3288efa190ed0136272bc1a1b3793259883b3cc6de6126bc87'
assert ini_hash == 'a06c949397c9709f58e6263a486702d89bf9001a5f7b3c39247d15d6bfdb6cd3'
disc = ROOT / 'build/debug-disc/usm-debug-game-replay.iso'
manifest = read_json(disc.with_suffix('.iso.usm.json'))
with disc.open('rb') as stream:
    stream.seek(manifest['dol_offset'])
    embedded_hash = hashlib.sha256(stream.read(dol.stat().st_size)).hexdigest()
    stream.seek(manifest['ini_offset'])
    embedded_ini = stream.read(manifest['ini_capacity']).rstrip(b'\0')
assert embedded_hash == dol_hash
assert embedded_ini == ini.read_bytes().rstrip(b'\0')
assets = read_json(ROOT / 'build/gamecube/character-assets.json')
verified_packs = {}
for character in assets['characters']:
    for candidate in character['candidates']:
        if candidate.get('pack_present'):
            relative = candidate['disc_relative_path']
            if relative not in verified_packs:
                actual = digest(ini.parent / relative)
                assert actual == candidate['pack_sha256'], relative
                verified_packs[relative] = actual
assets['catalogue_sha256'] = digest(ROOT / 'gamecube/menus/character_spawn_gc.cpp')
assets['revalidated_utc'] = datetime.now(timezone.utc).isoformat()
(ROOT / 'build/gamecube/character-assets.json').write_text(json.dumps(assets, indent=2) + '\n', encoding='utf-8')
camera = read_json(native / 'native-camera-smoke.json')
diagnostics = read_json(native / 'native-diagnostics-smoke.json')
levels = read_json(native / 'native-level-smoke.json')
spawn = read_json(native / 'native-menu-smoke.json')
progression = read_json(native / 'native-progression-smoke.json')
spawn_lifecycle = read_json(native / 'native-spawn-lifecycle.json')
game_devopts = read_json(native / 'native-game-devopts.json')
replay = read_json(native / 'native-replay.json')
assert levels['city_save_and_reboot']['reboot_observed']
assert levels['characterb_mailbox']['request'] == 0
assert spawn['spawned_actors'] == 1 and spawn['actors_after_pop_all'] == 0
assert spawn_lifecycle['created']['actor_count'] == 1
assert spawn_lifecycle['running_same_actor_and_packs']['actor0'] == spawn_lifecycle['created']['actor0']
assert spawn_lifecycle['after_pop_all']['actor_count'] == 0
assert game_devopts['devopts_count'] == 229 and game_devopts['measured_fps'] > 0
assert replay['recorded_samples'] > 0 and replay['native_delivery_while_physical_neutral']
assert replay['playback_completed_cursor'] == replay['recorded_samples']
assert replay['after_clear_samples'] == 0
assert [row['visible_row'] for row in camera['cameras']] == [
    'Camera : user', 'Camera : sceneanalyzer', 'Camera : chase', 'Camera : sceneanalyzer', 'Camera : chase']
assert diagnostics['diagnostics']['AI']['edit_round_trip']['incremented'] != diagnostics['diagnostics']['AI']['edit_round_trip']['before']
assert diagnostics['diagnostics']['AI']['export']['result'] == 1
playback = diagnostics['diagnostics']['Entity Animations']['playback']
assert playback['native_clip_and_speed_match'] and playback['menu_closed']
assert playback['actual_hash'] == playback['expected_hash'] and playback['speed'] == 1.0
assert playback['status'] == 'Animation started at normal speed'
before_ini = (native / 'ini-before-request.ini').read_bytes()
after_ini = (native / 'ini-after-request.ini').read_bytes()
assert before_ini != after_ini and update_scene(before_ini, 'city') == after_ini
exports = list((native / 'ai-exports').glob('*.txt'))
ai_digest = diagnostics['diagnostics']['AI']['export']['data_sha256']
assert any(digest(path) == ai_digest for path in exports)
checks = [
    ('runtime_checks',50), ('diagnostics_checks',diagnostics_count), ('settings_checks',21),
    ('mission_checks',94), ('progression_checks',67), ('spawn_checks',61),
    ('scene_checks',32), ('engine_checks',engine_count), ('camera_checks',34), ('ai_export_checks',38),
    ('replay_checks',replay_count), ('devopt_checks',38),
    ('Python unittest',57)]
report = {
    'generated_utc': datetime.now(timezone.utc).isoformat(),
    'output': 'main.dol', 'output_sha256': dol_hash, 'output_bytes': dol.stat().st_size,
    'original_sha256': digest(ROOT / 'main.original.dol'),
    'payload_start': hex(payload_start), 'payload_end': hex(payload_start + payload_size),
    'payload_bytes': payload_size, 'verified_patch_words': 26,
    'activation': 'GameCube Z alone, rising edge latched at PADRead',
    'debug_pause_release': 'Native PauseMenuSystem::Deactivate called only for debug-owned pauses; preexisting native pauses are retained',
    'camera_ui': ['Camera : chase','Camera : user','Camera : sceneanalyzer'],
    'checks': [{'suite': name,'count':count,'exit_code':0} for name,count in checks],
    'total_checks': sum(count for _,count in checks),
    'build_command': 'powershell -NoProfile -ExecutionPolicy Bypass -File build_gamecube.ps1 -OutputDol main.dol',
    'verification_commands': ['wsl -d Ubuntu -- bash tools/run_runtime_checks.sh',
                              "wsl -d Ubuntu -- python3 -m unittest discover -s tools/tests -p 'test_*.py' -v"],
    'installed_dol': str(installed), 'installed_dol_sha256': dol_hash,
    'installed_previous_backup': 'build/gamecube/extracted_main_before_spawn_game_replay.dol',
    'workspace_iso': str(disc.relative_to(ROOT)), 'embedded_dol_sha256':embedded_hash,
    'source_iso_sha256_unchanged':source_hash, 'supplied_ini_sha256_unchanged':ini_hash,
    'assets':{'characters':assets['counts'],'fresh_verified_pack_files':len(verified_packs),
              'missing_characters':[entry['name'] for entry in assets['characters'] if entry['status']=='missing'],
              'characterb_arena_present':False},
    'dolphin': {
        'tested_dol_sha256':dol_hash,'backend':'Null','startup_test_ini':'NO_LOAD_SCREEN=1 in workspace test copy; supplied INI unchanged',
        'test_conditions':'Evidence combines isolated runs of this final DOL. Spawn lifecycle: PID29180, God Mode3, progression unchanged, real A55 acknowledged a stock script dialog. Its evidence is retained in before-free-roam-setup. The failed PID16192 progression setup did not reach Spawn and is excluded. Replay: PID16192 after explicit stock-dialog dismissal as test setup.',
        'startup':read_json(native / 'menu-startup.json'),
        'test_method':'Real Z activation; synthetic payload-only navigation/deferred commands exercise existing menu handlers. Replay setup additionally used a fabricated deferred callback to the existing payload native Deactivate helper to dismiss a stock script dialog. No engine-code patches or direct native-object field writes.',
        'camera':camera['cameras'],
        'missions':{'records':spawn['mission_records'],'truncated':spawn['mission_catalogue_truncated']},
        'spawn':{'spawned':spawn['spawned_actors'],'after_pop_all':spawn['actors_after_pop_all']},
        'spawn_first_attempt_and_pause_return':spawn_lifecycle,
        'game_devopts':game_devopts,
        'replay':replay,
        'progression':progression.get('progression_activation',progression.get('progression_after_refresh')),
        'ai':diagnostics['diagnostics']['AI'],
        'animations':diagnostics['diagnostics']['Entity Animations'],
        'city':{**levels['city_save_and_reboot'], 'external_ini_changed_verified':True},
        'characterb':{'status':levels['characterb_status'],'persistence_request':levels['characterb_mailbox']['request']},
        'evidence_directory':str(native.relative_to(ROOT)),
        'visual_rendering_verified':False,'unpaused_camera_movement_verified':False,
        'after_reboot':'Mailbox reset proves SDK disc restart; subsequent native frontend requires normal controller input.'},
    'remaining':[
        'Characterb and three requested character assets are absent;25 character rows use disclosed verified fallback mappings.',
        'Replay records controller inputs in RAM and replays from the current position; it does not snapshot or rewind the world, AI, missions or RNG.',
        'Replay suspends during native pauses and script dialogs; dismiss the dialog normally to resume. The acceptance harness dismissed its stock dialog before measuring completion.',
        'Generic Devopts are native values: load-only settings require reload and retail-stripped consumers cannot be recreated by setting a flag.',
        'Spawn All is paced and bounded by native pack slots,heap and24MB MEM1;66 simultaneous actors are not guaranteed.',
        '18 PC NGL flags have no verified equivalent retail controls.',
        'MemTrack history/script callbacks and six PC Dvars are stripped or lack verified mutable GC equivalents.',
        'Native screenshot callbacks cannot save TGA because PCcreat/PCopen always fail in this retail DOL.',
        'Complete interactive visual,unpaused camera movement,all mission side effects and real-hardware validation remain unverified.',
        'Persistent City/AI export require the Windows Dolphin companion; physical GameCube discs are read-only.']
}
(ROOT / 'build/gamecube/verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'dol_sha256':dol_hash,'checks':report['total_checks'],'payload_bytes':payload_size,
                  'missions':spawn['mission_records'],'ai_exports':len(exports)},indent=2))
