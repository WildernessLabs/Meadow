# Development Workflow

## Track Execution Order
Tracks are executed sequentially as listed in `tracks.md`. Each track has phases that are also sequential.

## Validation Strategy
1. **Build validation**: Code compiles without errors
2. **Emulator validation**: Runs correctly in Renode emulator
3. **Hardware validation**: Runs correctly on physical F7FeatherV2 / CCMv2 (manual step)

## Commit Strategy
- Use conventional commits: `feat(scope): description`, `fix(scope): description`
- Never include AI attribution in commits
- Never push without explicit approval
- Commit working milestones within each phase

## Phase Completion Protocol
1. All tasks in the phase are checked off
2. Emulator validation passes (where applicable)
3. User signs off before proceeding to next phase

## Deployment for Testing
- **Emulator**: `./run.sh` in Meadow.OS.Emulator, then `meadow config route socket://localhost:4242`
- **App deploy**: `meadow app run` from app project directory (uses Meadow.CLI socket branch)
- **Hardware**: Flash firmware via SWD, deploy app via USB with Meadow.CLI

## Key Branches
- Work on feature branches per track
- Merge to develop/main after track completion and validation
