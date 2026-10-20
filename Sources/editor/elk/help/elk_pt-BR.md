# Blitzkrieg ELK

O ELK, o kit de localização do Blitzkrieg, é onde você traduz os textos do jogo. Ele mostra cada texto do jogo com o seu original, você digita a tradução, marca em que ponto cada texto está e depois coloca a sua tradução no jogo como um mod de idioma.

## Iniciando o ELK

- Iniciado sozinho, o ELK abre o banco de dados que você usou por último.
- ELK path/to/elk.xml abre esse banco de dados.
- ELK --short é o layout do tradutor: ele abre o elk.xml ao lado do programa, antes o atualiza com o arquivo de atualização .upd mais recente que houver ali e mostra uma única barra de ferramentas com os cinco filtros fixos. Ele sempre inicia da mesma forma.
- ELK --developer, ou nenhuma opção, é o layout estendido com todos os menus, os filtros nomeados que você mesmo cria e a lista de bancos de dados recentes.
- O idioma dos menus, das mensagens e desta ajuda do ELK é escolhido em Ferramentas > Idioma. O ELK o memoriza; Sistema segue o idioma do seu sistema operacional.

## Abrindo e salvando

Um banco de dados é uma pasta com um arquivo elk.xml e uma pasta para cada parte do jogo, como game_data_base. Abra um com Arquivo > Abrir (Ctrl+O) ou escolha-o em Arquivo > Recentes.

Você não perde trabalho: o ELK salva o texto que você estava editando quando você passa para outro texto, em Arquivo > Salvar (Ctrl+S), em Arquivo > Fechar, ao sair e a cada 30 segundos enquanto houver algo não salvo. A linha de status na parte inferior indica quando o texto tem alterações não salvas.

## A árvore de textos

O lado esquerdo é a árvore de todos os textos do jogo, em pastas como o jogo as mantém. Clique em um texto para exibi-lo: seu original no alto, sua descrição (uma nota para tradutores, quando o texto tem uma) abaixo dele e sua tradução mais abaixo. O original e a descrição são somente leitura, mas você pode selecionar e copiar deles. Abra uma pasta pela seta ou com um clique duplo.

Cada linha tem a cor do seu estado, e uma pasta tem a cor do texto menos adiantado que há nela, então uma pasta vermelha ainda tem trabalho.

## Estados

Cada texto tem um de quatro estados, escolhido com os botões entre o original e a tradução:

- Não traduzido (vermelho): nada foi feito ainda.
- Desatualizado (amarelo): o original mudou em uma atualização depois de traduzido. Confira a tradução e defina-a de novo. Somente uma atualização ou uma importação de planilha define este estado.
- Traduzido (verde): traduzido, ainda não conferido. Digitar em uma tradução define este estado.
- Aprovado (azul): traduzido e conferido.

## Filtros e navegação

O filtro escolhe quais textos os comandos de Navegar percorrem. A árvore esmaece os textos que o filtro deixa de fora, mas ainda os mostra.

- Os cinco filtros fixos são Todos, Não traduzidos, Desatualizados, Traduzidos e Aprovados.
- Navegar > Próximo (Ctrl+N) e Anterior (Ctrl+P) vão para o texto seguinte ou anterior que o filtro mantém, e o cursor vai direto para a tradução. No layout estendido, Primeiro e Último vão para as extremidades.
- Recolher item fecha as pastas que você deixa para trás ao navegar, de modo que só o caminho até o texto atual fique aberto.
- No layout estendido, Navegar > Criar filtro... cria seus próprios filtros: um nome, os estados que eles mantêm, Somente alterados e condições de pasta como medals&german (cada parte deve estar no caminho da pasta).

## Localizar

Editar > Localizar... (Ctrl+F, em Ferramentas no layout curto) pesquisa os originais, as descrições e as traduções de todos os textos na ordem da árvore. Localizar próximo vai ao próximo ponto; Diferenciar maiúsculas, Somente palavras inteiras e Para cima ou Para baixo funcionam como em outros programas. A linha de status diz em qual campo e onde o texto foi encontrado.

## Estatísticas

Exibir > Estatísticas do ELK conta os textos, as palavras e os caracteres dos originais e das traduções, no total e por estado, para você ver quanto falta. Ao lado estão as contagens de quando o banco de dados foi fechado pela última vez após contar, para você ver quanto mudou desde então.

Exibir > Árvore do ELK oculta ou mostra a lista de textos, e Exibir > Barra de status (layout estendido) a linha de status; o ELK memoriza ambos.

## Verificação ortográfica

Ferramentas > Ortografia (F7) verifica desde o início a tradução que você está editando. Ela para em cada palavra escrita incorretamente e coloca sua melhor sugestão na caixa Alterar para; clique em outra sugestão ou digite a sua. Ignorar pula a palavra desta vez, Ignorar tudo a pula até você fechar o ELK, Alterar a substitui pelo texto de Alterar para, e Alterar tudo a substitui em toda a tradução. Verificar novamente recomeça. Uma alteração é uma edição como uma digitada e é salva como ela. Abaixo, cada palavra escrita incorretamente é listada com suas sugestões; Adicionar ao dicionário guarda uma palavra, como um nome, no seu próprio arquivo de dicionário custom.dic, para que ela nunca mais seja marcada.

A verificação ortográfica precisa de um dicionário Hunspell, um arquivo .dic e um .aff do seu idioma, por exemplo do LibreOffice. Coloque-os na pasta dictionaries ao lado do ELK, ou escolha outra pasta e o idioma em Ferramentas > Opções de ortografia.... Enquanto nenhum dicionário for encontrado, Ortografia fica desativada e diz por quê.

## Planilhas

Você pode traduzir em um programa de planilhas e trazer o resultado de volta. No layout curto, esses comandos, a exportação e Executar o jogo ficam em Ferramentas e na barra de ferramentas.

- Arquivo > Exportar para planilha... grava cada texto, com seu caminho, original, tradução, estado e descrição, em uma pasta de trabalho .xlsx (o padrão) ou em um arquivo .csv UTF-8.
- Arquivo > Importar de planilha... lê uma planilha assim, ou uma antiga .xls do ELK original. Cada linha com uma tradução define a tradução daquele texto. Uma linha cujo original não corresponde mais ao banco de dados marca o texto como Desatualizado. Linhas sem tradução são ignoradas. A linha de status diz quantas linhas foram lidas e alteradas.
- Mantenha a coluna Path como está: ela diz ao ELK a qual texto uma linha pertence.

## Atualizando os originais

Quando os textos do jogo mudam, traga os novos originais para o banco de dados aberto. Um texto cujo original mudou passa a Desatualizado se estava traduzido ou aprovado; caso contrário, a Não traduzido; suas traduções permanecem.

- Arquivo > Importar do jogo... lê os textos da pasta de idioma de um jogo (a pasta que contém Textes). Textos novos são adicionados; nada é excluído.
- Arquivo > Importar de PAK/UPD... lê um arquivo .pak ou .upd. O nome dele escolhe a parte do banco de dados que ele atualiza: game_12.upd atualiza game. Um .upd é uma atualização completa e também remove os textos que ele não tem mais.
- A linha de status diz quantos textos foram importados. Importar o mesmo arquivo de novo não altera nada.

## Colocando a tradução no jogo

- Arquivo > Exportar para mod de idioma... grava sua tradução como um mod: digite um nome, e o ELK grava mods/<nome>/data/texts.pak na pasta do jogo. Este é o caminho seguro: o jogo lê um mod antes de seus próprios arquivos de texto, e o jogo original fica intacto.
- No layout estendido, quando o filtro atual deixa textos de fora, a caixa de exportação oferece gravar somente os textos que o filtro mantém, por exemplo só os Aprovados. Os textos deixados de fora não estão nesse mod.
- No jogo, escolha o mod em Opções > Configurações > Jogabilidade > Idioma. O jogo troca de idioma na hora e memoriza a escolha. Escolha ali o idioma original para voltar.
- Arquivo > Instalar no jogo... grava Data/Texts.pak no próprio jogo, como o ELK original fazia. Se o jogo tiver arquivos de texto soltos que o esconderiam, o ELK avisa você e oferece o mod no lugar.
- Arquivo > Executar o jogo exporta o mod de novo com suas últimas edições e inicia o jogo com ele, em um perfil de teste separado, de modo que seus próprios jogos salvos e configurações não são tocados.
- O ELK procura o jogo na pasta de onde ele é executado.

## Operações longas

As exportações, as importações e uma busca que precisa ler todos os textos são executadas em segundo plano. Uma janela Trabalhando mostra o que o ELK está fazendo e até onde foi, e o resto do ELK espera até terminar. Cancelar para com segurança: uma exportação cancelada não grava nada, e uma importação cancelada mantém as linhas já lidas, com cada arquivo íntegro, então você pode importar a planilha de novo depois.

## Teclas

- Ctrl+O abrir, Ctrl+S salvar
- Ctrl+N ou Alt+N próximo texto, Ctrl+P ou Alt+P texto anterior (Alt somente enquanto o cursor não está na tradução)
- Ctrl+F localizar
- F7 ortografia
- F1 esta ajuda
