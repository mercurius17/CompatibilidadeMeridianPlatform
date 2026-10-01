# Licenças, autoria e componentes externos

Este snapshot deriva do runtime Skyrim Platform/Aetherius e do client Aetherius. Foram preservados LICENSE GPL v3 da raiz e dos componentes originais, avisos de autoria nos fontes e licenças próprias presentes nos subdiretórios. Não atribuir o projeto original ou o Meridian aos autores desta integração.

O SDK Meridian copiado (`Settings.h`, `ViewAPI.h`, `ViewDllLoader.h`) é mantido inalterado com LICENSE-MIT e PROVENANCE em `source/runtime/skyrim-platform/src/platform_se/skyrim_platform/MeridianUIAPI`. O provider/runtime Meridian não foi recompilado, alterado ou distribuído neste repositório.

`licenses/dependencies` arquiva arquivos copyright disponibilizados pelos ports usados no ambiente local. Esse inventário inclui dependências da toolchain completa, inclusive algumas que não entram no target Meridian. Não inferir linkagem pelo inventário; consulte os manifests CMake/vcpkg e os relatórios PE. Os avisos não substituem a licença própria de cada componente nem garantem que a metadata de todo port seja perfeita.

Não redistribuídos aqui: executáveis do Skyrim, SKSE, MO2, runtime/CEF Meridian, CEF legado, libnode prebuilt, mods de terceiros, bibliotecas instaladas do vcpkg, tokens, credenciais operacionais ou saves pessoais.

Os sources originários podem conter fixtures de testes e assets versionados na base, não extraídos da instalação pessoal do jogo. Em especial, OpenSSLSignerTest contém chaves de fixture incorporadas no teste; não são credenciais operacionais e não devem ser utilizadas em produção. OpenSSLSigner.cpp contém somente delimitadores de PEM usados pelo parser. Nenhum segredo de conta foi acrescentado à publicação.

Ao preparar uma distribuição completa para usuários, preservar todos os avisos aplicáveis e publicar as fontes correspondentes à revisão efetivamente distribuída. Não distribuir o overlay incompleto como se contivesse todos os componentes externos.
