# real-hand runs

Reader: firmware integrator validating the physical S3 hand.

Each dated directory contains raw serial evidence for the real S31 → S3 `game_hand` path: ESP-NOW commands, servo driver calls, TOF baselines, DROP frames, ACKs and close sequence. A captured PWM log proves that the firmware applied the command; it does not by itself prove the physical lid reached the expected position.

Do not replace raw logs with summaries. Put interpretation in the dated README.
