# Blitzkrieg ELK

ELK, el kit de localización de Blitzkrieg, es donde se traducen los textos del juego. Muestra cada texto del juego con su original, usted escribe la traducción, marca en qué punto está cada texto y luego incorpora su traducción al juego como mod de idioma.

## Iniciar ELK

- Iniciado solo, ELK abre la base de datos que usó por última vez.
- ELK path/to/elk.xml abre esa base de datos.
- ELK --short es el diseño del traductor: abre el elk.xml junto al programa, antes lo actualiza con el archivo de actualización .upd más reciente que haya allí y muestra una sola barra de herramientas con los cinco filtros fijos. Siempre se inicia de la misma manera.
- ELK --developer, o ninguna opción, es el diseño ampliado con todos los menús, los filtros con nombre que usted crea y la lista de bases de datos recientes.
- El idioma de los menús, los mensajes y esta ayuda de ELK se elige en Herramientas > Idioma. ELK lo recuerda; Sistema sigue el idioma de su sistema operativo.

## Abrir y guardar

Una base de datos es una carpeta con un archivo elk.xml y una carpeta por cada parte del juego, como game_data_base. Abra una con Archivo > Abrir (Ctrl+O) o elíjala en Archivo > Recientes.

No pierde trabajo: ELK guarda el texto que estaba editando cuando pasa a otro texto, con Archivo > Guardar (Ctrl+S), con Archivo > Cerrar, al salir y cada 30 segundos mientras haya algo sin guardar. La línea de estado de abajo indica cuándo el texto tiene cambios sin guardar.

## El árbol de textos

A la izquierda está el árbol de todos los textos del juego, en carpetas como el juego los guarda. Haga clic en un texto para mostrarlo: su original arriba, su descripción (una nota para traductores, si el texto la tiene) debajo y su traducción más abajo. El original y la descripción son de solo lectura, pero puede seleccionar y copiar de ellos. Abra una carpeta con su flecha o haciendo doble clic.

Cada fila tiene el color de su estado, y una carpeta tiene el color del texto menos avanzado que contiene, así que una carpeta roja aún tiene trabajo.

## Estados

Cada texto tiene uno de cuatro estados, que se elige con los botones entre el original y la traducción:

- Sin traducir (rojo): todavía no se ha hecho nada.
- Desactualizado (amarillo): el original cambió en una actualización después de traducirse. Revise la traducción y vuelva a establecerla. Solo una actualización o una importación de hoja de cálculo establece este estado.
- Traducido (verde): traducido, aún sin revisar. Escribir en una traducción establece este estado.
- Aprobado (azul): traducido y revisado.

## Filtros y exploración

El filtro elige los textos que recorren los comandos de Explorar. El árbol atenúa los textos que el filtro deja fuera, pero los sigue mostrando.

- Los cinco filtros fijos son Todos, Sin traducir, Desactualizados, Traducidos y Aprobados.
- Explorar > Siguiente (Ctrl+N) y Anterior (Ctrl+P) van al texto siguiente o anterior que el filtro conserva, y el cursor entra directamente en la traducción. En el diseño ampliado, Primero y Último van a los extremos.
- Contraer elemento cierra las carpetas que deja atrás al explorar, de modo que solo queda abierto el camino hasta el texto actual.
- En el diseño ampliado, Explorar > Crear filtro... crea sus propios filtros: un nombre, los estados que conservan, Solo modificados y condiciones de carpeta como medals&german (cada parte debe estar en la ruta de la carpeta).

## Buscar

Edición > Buscar... (Ctrl+F, en Herramientas en el diseño corto) busca en los originales, las descripciones y las traducciones de todos los textos, en el orden del árbol. Buscar siguiente va al siguiente lugar; Distinguir mayúsculas, Solo palabras completas y Arriba o Abajo funcionan como en otros programas. La línea de estado indica en qué campo y dónde se encontró el texto.

## Estadísticas

Ver > Estadísticas de ELK cuenta los textos, las palabras y los caracteres de los originales y las traducciones, en total y por estado, para que vea cuánto falta. Junto a ellos están los recuentos tal como estaban cuando la base de datos se cerró por última vez tras contar, para que vea cuánto ha cambiado desde entonces.

Ver > Árbol de ELK oculta o muestra la lista de textos, y Ver > Barra de estado (diseño ampliado) la línea de estado; ELK recuerda ambos.

## Revisión ortográfica

Herramientas > Ortografía (F7) revisa desde el principio la traducción que está editando. Se detiene en cada palabra mal escrita y pone su mejor sugerencia en el cuadro Cambiar por; haga clic en otra sugerencia o escriba la suya. Omitir salta la palabra esta vez, Omitir todo la salta hasta que cierre ELK, Cambiar la sustituye por el texto de Cambiar por, y Cambiar todo la sustituye en toda la traducción. Revisar de nuevo empieza otra vez. Un cambio es una edición como una escrita y se guarda como ella. Debajo, cada palabra mal escrita aparece con sus sugerencias; Añadir al diccionario guarda una palabra, como un nombre, en su propio archivo de diccionario custom.dic, para que no vuelva a marcarse nunca.

La revisión ortográfica necesita un diccionario Hunspell, un archivo .dic y uno .aff de su idioma, por ejemplo de LibreOffice. Póngalos en la carpeta dictionaries junto a ELK, o elija otra carpeta y el idioma en Herramientas > Opciones de ortografía.... Mientras no se encuentre un diccionario, Ortografía está desactivada y explica por qué.

## Hojas de cálculo

Puede traducir en un programa de hojas de cálculo y traer el resultado de vuelta. En el diseño corto estos comandos, la exportación y Ejecutar el juego están en Herramientas y en la barra de herramientas.

- Archivo > Exportar a una hoja de cálculo... escribe cada texto, con su ruta, original, traducción, estado y descripción, en un libro .xlsx (el predeterminado) o un archivo .csv UTF-8.
- Archivo > Importar de una hoja de cálculo... lee una hoja así, o una antigua .xls del ELK original. Cada fila con traducción establece la traducción de ese texto. Una fila cuyo original ya no coincide con la base de datos marca el texto como Desactualizado. Las filas sin traducción se omiten. La línea de estado indica cuántas filas se leyeron y cambiaron.
- Deje la columna Path como está: indica a ELK a qué texto pertenece cada fila.

## Actualizar los originales

Cuando cambian los textos del juego, traiga los nuevos originales a la base de datos abierta. Un texto cuyo original cambió pasa a Desactualizado si estaba traducido o aprobado; si no, a Sin traducir; sus traducciones se conservan.

- Archivo > Importar del juego... lee los textos de la carpeta de idioma de un juego (la carpeta que contiene Textes). Se añaden los textos nuevos; no se elimina nada.
- Archivo > Importar de PAK/UPD... lee un archivo .pak o .upd. Su nombre elige la parte de la base de datos que actualiza: game_12.upd actualiza game. Un .upd es una actualización completa y también elimina los textos que ya no tiene.
- La línea de estado indica cuántos textos se importaron. Importar de nuevo el mismo archivo no cambia nada.

## Llevar la traducción al juego

- Archivo > Exportar a un mod de idioma... escribe su traducción como mod: escriba un nombre y ELK escribe mods/<nombre>/data/texts.pak en la carpeta del juego. Es la forma segura: el juego lee un mod antes que sus propios archivos de texto y el juego original queda intacto.
- En el diseño ampliado, cuando el filtro actual deja textos fuera, el cuadro de exportación ofrece escribir solo los textos que el filtro conserva, por ejemplo solo los Aprobados. Los textos que deja fuera no están en ese mod.
- En el juego, elija el mod en Opciones > Ajustes > Jugabilidad > Idioma. El juego cambia de idioma al instante y recuerda la elección. Elija allí el idioma original para volver.
- Archivo > Instalar en el juego... escribe Data/Texts.pak en el propio juego, como hacía el ELK original. Si el juego tiene archivos de texto sueltos que lo ocultarían, ELK le avisa y le ofrece el mod en su lugar.
- Archivo > Ejecutar el juego vuelve a exportar el mod con sus últimas ediciones e inicia el juego con él, en un perfil de prueba aparte, de modo que sus propias partidas y ajustes no se tocan.
- ELK busca el juego en la carpeta desde la que se ejecuta.

## Operaciones largas

Las exportaciones, las importaciones y una búsqueda que tiene que leer todos los textos se ejecutan en segundo plano. Una ventana Trabajando muestra lo que hace ELK y cuánto ha avanzado, y el resto de ELK espera a que termine. Cancelar lo detiene con seguridad: una exportación cancelada no escribe nada y una importación cancelada conserva las filas ya leídas, con cada archivo íntegro, así que puede importar la hoja de nuevo más tarde.

## Teclas

- Ctrl+O abrir, Ctrl+S guardar
- Ctrl+N o Alt+N texto siguiente, Ctrl+P o Alt+P texto anterior (Alt solo mientras el cursor no está en la traducción)
- Ctrl+F buscar
- F7 ortografía
- F1 esta ayuda
