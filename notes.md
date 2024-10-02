Found device: ELK-BLEDOM06 - 2DC0DCE8-324D-3A88-1DED-D455BFF46107
Found device: ELK-BLEDOM02 - 18FC4642-ADEA-7B0A-39BE-AB6C8961C82C
BE:67:00:AC:C8:82 ELK-BLEDOM02
BE:67:00:6A:B5:A6 ELK-BLEDOM06



7e [command length] [action byte] [data type] [red component] [green component] [blue component] [additional data] EF

OFF: await self.send_msg("07", "05", "03", R, G, B, "10")
    

I also worked out that 7e 07 04 ff 00 01 02 01 ef turns it on.
7e 07 04 ff 00 01 02 01 ef