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
- Se nenhuma palavra natural em português couber no CAP, **deixe o inglês** (nada de abreviações como "Cpr").
- Mensagens técnicas do motor (erros, debug, nomes de arquivo) ficam em inglês.

## Estilo

- Português do Brasil natural, de jogo de aventura para todas as idades. "Você" (não "tu").
- **Arche**: menina de 10 anos, animada, impulsiva, fala informal ("pra", "tá", "né" com moderação).
- **Sana**: tímida e gentil, educada, frases mais suaves.
- **Stella**: menina rica e esnobe, fala com ar de superioridade ("Ora essa!", "Hmpf!"), mas é engraçada.
- Adultos: tom normal; o pai e a mãe da Arche são carinhosos.
- **Chiffon** (criança pequena, fala errado): troca r por l ("lemblar", "podel", "mestle"), "vochê".
- Fala arcaica ("thee / thy / thou"): use "tu" / "teu" / "vós" com ar solene.
- Crianças que trocam letras em outras falas: troque r por l, como a Chiffon.
- "magic rock paper scissors" = "jokenpô mágico".
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
| Class President / President | a Representante da Turma / a Representante (é uma menina) |
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
| Woosh, sploosh, thunk, SIZZ! (feitiço da Arche) | Fuuu, splash, tum, TSSS! |
| %d copper (moeda) | %d de cobre |
| Warm Breeze Inn | Pousada Brisa Morna |
| Merkid(s) / merfolk | merkid(s) / povo do mar |
| Ribbit (som dos merkids) | Croac |
| Underloch (lugar) | Underloch |
| Mothbee | Mariposabelha |
| Preserver | Preservador(a) |
| Wind Hill | Colina do Vento |
| Settings | Configurações |
| Level | Nível |
| Mode | Modo |
| Voice / Volume | Voz / Volume |
| Error | Erro |
| Menu | Menu |
| Save / Load | Salvar / Carregar |
| Item(s) | Item / Itens |
| Equip / Equipment | Equipar / Equipamento |

## Magias e golpes

Em falas gritadas ficam em MAIÚSCULAS como no inglês; em menus, com iniciais maiúsculas ("Água Curativa II").
Os números romanos (II, III) continuam.

| Inglês | Português |
|---|---|
| Violet Bloom | Flor Violeta |
| Rock Fall | Queda de Rochas |
| Thunderbolt | Raio |
| Wind Gust | Rajada de Vento |
| Water Shoot | Jato d'Água |
| Water Blow | Golpe d'Água |
| Fire Handling | Domínio do Fogo |
| Fire Wall | Muralha de Fogo |
| Heal Water | Água Curativa |
| Heal Mist | Névoa Curativa |
| Sleep Mist | Névoa do Sono |
| Buster Tornado | Tornado Devastador |
| Turbid Squall | Tempestade Turva |
| Diamond Dust / Diamonddust | Pó de Diamante |
| Phoenix | Fênix |
| Inferno | Inferno |
| Explosion | Explosão |
| Shield | Escudo |
| Force Field | Campo de Força |
| Mirror Image | Imagem Espelhada |
| Illusion / Disillusion | Ilusão / Desilusão |
| Life Force | Força Vital |
| Revival | Reviver |
| Antidote | Antídoto |
| Teleportation | Teletransporte |
| Chaser | Perseguidor |
| Interceptor | Interceptador |
| Disintegrate | Desintegrar |
| Cerberus | Cérbero |
| Saber / Blade | Sabre / Lâmina |
| Splash / Wave | Respingo / Onda |
| Unison | Unísono |
| Dark Graviton | Gráviton Sombrio |
| Dark Hold | Prisão Sombria |
| Dark Stalker | Perseguidor Sombrio |
| Conjure Servant | Invocar Servo |
| Wind Thrust | Estocada de Vento |
| Ghost Blaster | Rajada Fantasma |
