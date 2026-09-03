# Região por IP

O painel de jogadores possui um ponto de integração com cache para exibir apenas
o país associado ao IP. Por privacidade e segurança, a versão pública do Tenebris
não envia automaticamente endereços de jogadores a serviços externos e não
inclui chaves privadas no código. Sem um provedor GeoIP explicitamente
configurado, o painel mostra **Desconhecida** e mantém esse resultado em cache.

Uma integração futura deve resolver os dados fora da thread de renderização,
alimentar o cache somente quando um jogador/IP mudar e continuar funcionando
quando a rede ou o provedor estiver indisponível.
