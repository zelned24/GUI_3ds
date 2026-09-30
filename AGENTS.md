# Agentes — PokéRogue para Old 3DS XL

1. Objetivo: migración real de PokéRogue a C++/devkitARM/libctru/Citro2D para Old 3DS XL. No mantener ni reconstruir el editor web/Studio.
2. Runtime en project/; importación/conversión/generación sin UI en tools/js/ y scripts/. Azahar valida el programa; memoria/rendimiento requieren Old 3DS físico.
3. Upstream pinned → import → canonical → override explícito → runtime. No pegar TypeScript/Phaser ni inventar reglas, datos o rutas de assets.
4. Conservar repository, revision, sourcePath/symbol, SHA-256, schema y metadata desconocida. Distinguir faltante upstream de capacidad no soportada e importación inválida.
5. Separar datos, combate, presentación C++ y actualización. UI → command → engine → event → binding. Pantallas 400×240 y 320×240.
6. RNG y outputs deterministas; sin reloj ni aleatoriedad no seeded en resultados, IDs o generación. Fixtures exclusivamente para tests, nunca producción.
7. Buscar/reutilizar antes de crear sistemas; inspeccionar referencias antes de borrar. Conservar historial y cambios de otras IA.
8. Reparto: este agente combate/gameplay; otra IA presentación/assets. Coordinar archivos compartidos antes de editar simultáneamente.
9. Por instrucción del usuario, tests y compilación del programa se ejecutan al final. Conversión PNG→t3x y revisión estática permitidas. Escribir pruebas relevantes; no ocultar fallos ni cambiar asserts para obtener PASS.
10. No declarar Classic completo sin progresión, encuentros, combate, rewards, save/continue/export, win/lose/summary y evidencia de hardware. OTA requiere catálogo cargable y firma, no solo texturas.
11. Leer README.md y docs/MIGRATION_STATUS.md, luego únicamente la sección pertinente de docs/progress/POKEROGUE_3DS_REMAINING_WORK.md. No cargar todos los catálogos/docs por defecto.
