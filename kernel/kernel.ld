OUTPUT_FORMAT("binary")
ENTRY(_start)

phys = 0x00001000;

SECTIONS
{
  . = phys;  /* Définit l'adresse de départ Ã  0x1000 */

  .text : AT(phys)
  {
    *(.text)  /* Code exécutable */
    *(.rodata) /* Données en lecture seule */
    *(.rodata*) /* Autres donnÃ©es en lecture seule */
  }

  .data :
  {
    *(.data)  /* Données initialisées */
  }

  .bss :
  {
    *(.bss)  /* Données non initialisées */
    *(COMMON) /* Variables communes */
  }

  /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) } /* Ignore les sections inutiles */
}
