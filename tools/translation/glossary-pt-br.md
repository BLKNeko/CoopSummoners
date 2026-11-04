# Glossário e guia de estilo — tradução PT-BR

Usado por quem traduz ou revisa `data/lang/pt-br.tsv`. Termos em inglês aqui são só os nomes do jogo
(nada do roteiro).

## Regras de formato (obrigatórias — `merge.py` rejeita a linha se quebrar)

- **Mantenha os códigos do jogo exatamente como estão**: `%s`, `%d`, `% 3d`, `%04d` (onde entra um nome ou
  número), `%mN%`, `%w`, `@@X`, `@@Z`, `@@F1`… (nome da tecla configurada), `#1`, `#2`.
- `%n` é **quebra de linha**. Pode mudar de lugar para a frase ficar natural; mantenha mais ou menos a mesma
  quantidade de linhas, e cada linha com tamanho parecido com a do inglês.
- **CAP > 0** = a frase é copiada para um espaço fixo: o texto em português **não pode passar de CAP
  caracteres** (letra acentuada conta 1). Se não couber, abrevie.
- Só caracteres do Windows-1252 (todo o português cabe). Use aspas retas `"` e `'`, e `...` em vez de `…`.
- Rótulos (KIND = label: menus, nomes) devem ficar curtos — perto do tamanho do inglês.

## Estilo

- Português do Brasil natural, de jogo de aventura para todas as idades. "Você" (não "tu").
- **Arche**: menina de 10 anos, animada, impulsiva, fala informal ("pra", "tá", "né" com moderação).
- **Sana**: tímida e gentil, educada, frases mais suaves.
- **Stella**: menina rica e esnobe, fala com ar de superioridade ("Ora essa!", "Hmpf!"), mas é engraçada.
- Adultos: tom normal; o pai e a mãe da Arche são carinhosos.
- Interjeições: Yay → Eba!/Oba!; Awww → Aaah...; Hmm → Hmm; Huh? → Hã?; Ow → Ai!; Ugh → Argh/Ugh.
- Palavras em MAIÚSCULAS para ênfase continuam em maiúsculas.
- `%s` com nomes de itens: evite artigo com gênero ("um %s"/"uma %s"); reescreva:
  "Ah, você achou um %s!" → "Ah, você achou: %s!" ou "Ah, olha só o que você achou: %s!".
- Piadas e trocadilhos: adapte para algo que funcione em português, em vez de traduzir ao pé da letra.

## Nomes que NÃO mudam

Personagens e lugares próprios: Arche, Sana, Stella, Colm, Chiffon, Merrin, Leon, Chupil, Rowen, Sophia,
Kanon, Alex, Marina, Remi, Selene, Ellie, Louise, Algorn, Silpheed, Yamanba, Korat, Chartreux, Barness,
Tonkiness, Scotshold, Scotsholm, Mayberk, Carudale, Abyssinia, Dalmatia, Towarin, Poanet, Renoria,
Minasa-Ratis. Siglas: HP, MP, CP.

## Termos

| Inglês | Português |
|---|---|
| Elemental Stone(s) | Pedra(s) Elemental(is) |
| Great Stones | Grandes Pedras |
| Wind / Fire / Water / Earth (elementos) | Vento / Fogo / Água / Terra |
| Crest(s) (emblema mágico) | Brasão / Brasões |
| Spell / Magic | Magia |
| Mage | Mago(a) |
| Witch / Dark Witch | Bruxa / Bruxa das Trevas |
| Dark World | Mundo das Trevas |
| Town of Tonkiness | Cidade de Tonkiness |
| Weathervane Tower | Torre do Cata-vento |
| Tower | Torre |
| Korat Village / Barness Village | Vila Korat / Vila Barness |
| Village | Vila |
| Chartreux City | Cidade de Chartreux |
| Chartreux Catacombs / Catacombs | Catacumbas de Chartreux / Catacumbas |
| Chartreux Guardpost | Posto da Guarda de Chartreux |
| Loch Carudale | Lago Carudale |
| Mayberk Manor | Mansão Mayberk |
| Minasa-Ratis Magic School / Magic School | Escola de Magia Minasa-Ratis / Escola de Magia |
| Shrine | Santuário |
| North Gate | Portão Norte |
| First Street | Primeira Rua |
| Valley | Vale |
| Inn | Pousada |
| Dad / Mom, Mama | Papai / Mamãe (fala da Arche); pai / mãe (narração) |
| Father / Mother | Pai / Mãe |
| Ms. X / Miss X | Srta. X (professora: Profa. X quando for a professora) |
| Mister Mayor / Mayor | Senhor Prefeito / Prefeito |
| Mister Guard / Guard | Seu Guarda / Guarda |
| Sergeant | Sargento |
| Headmaster | Diretor(a) |
| Class President / President | Representante da Turma / Representante |
| Topah Herb / Herb | Erva Topah / Erva |
| Mentama Candy / Candy | Bala Mentama / Bala |
| Capsule Machine | Máquina de Cápsulas |
| Mark of Heroism | Marca do Heroísmo |
| Black Steel | Aço Negro |
| Grand Gourmand | Grande Gourmet |
| Royal Scarlet | Royal Scarlet (nome próprio) |
| Sword / Dagger / Staff | Espada / Adaga / Cajado |
| Armor / Cape / Ring | Armadura / Capa / Anel |
| Dress / Smock / Hairband / Beret | Vestido / Avental / Tiara / Boina |
| Slime | Slime |
| Myconid | Miconídeo |
| Skeleton | Esqueleto |
| Ghost | Fantasma |
| Bat | Morcego |
| Harpy | Harpia |
| Kobold / Org / Munchkin | Kobold / Org / Munchkin |
| Slash / Thrust | Corte / Estocada |
| Unison | Unísono |
| Settings | Configurações |
| Level | Nível |
| Mode | Modo |
| Voice / Volume | Voz / Volume |
| Error | Erro |
| Menu | Menu |
| Save / Load | Salvar / Carregar |
| Item(s) | Item / Itens |
| Equip / Equipment | Equipar / Equipamento |
