----------------------------------------------------------------
						solar_system leírás
----------------------------------------------------------------

FONTOS:

	- A solution fájl a "Grafikagyakorlatok.sln" mivel az átnevezések után problémám volt a futtatásokkal
	- A projekt (.vcxproj) még mindig a Gyak3.1 néven fut, ugyanemiatt; elvileg ennek nem kéne gondot okoznia
	- Maga a .cpp fájl "hazi2" néven van megkülönböztetve
	- A textúráknak nincsen külön mappájuk, ami miatt bocsánatot kérek, így egy kicsit nehezebben átlátható az a mappa


A háziban a következőket sikerült teljesíteni saját megítélésre

Alapok:

	- A nap az origóban, azaz középen
	- A nap fényt áraszt, világít maga köré, és emisszív kinézete van
	- Van valamennyi ambiens fény,  hogy látszódjanak a bolygóknak a másik felük is
	
	- A nap körül 8 bolygó kering
	- A bolygók forognak a saját tengelyük körül is (kocka alakúak az agszerűség kedvéért)
	
	- Tudunk mozogni a naprendszerben a kamerával
	
Továbbiak:

	- A bolygókat igyekeztem nagyjából méretarányosra a valódi adatok szerint méretezni
	- A bolygókra kerestem UV textúrákat, rámappeltem polárkoordinátás eljárásokkal, melyekkel talán könnyebb őket felismerni (és nem olyan csúnyák :) remélhetőleg)
	
	- A kamerának van ki-be kapcsolható fénye
	
	- Cubemap implementálva van, melynek az oldalain szintén textúra van a jobb látvány miatt
	- A cubemap velünk mozog, így sosem ütközünk neki
	
	- A bolygóknak van hitboxa, melyet egy collision függvény folyamatosan figyel
	- Ha ütközünk, megáll a játék, majd újraindítható
	
Billentyűk kezeléséhez segítség:

	- A flashlight bekapcsolása: (F)
	- Előre, hátra, balra, jobbra menet: (W, A, S, D)
	- Fel, le lebegés: (SPACE, LEFT_SHIFT)
	- Az egér görgőivel lehet a mozgás sebességét állítani (fel -> gyorsabb, le -> lassabb)
	- Újraindítás, ha ütközés volt: (ENTER)
	- Kilépés: (ESC)
	
Megjegyzés:

	- Ütközés után nem lehet rögtön kilépni (ESC)-el, csak ha a játékos újraéledt, miután megnyomta az (ENTER)-t
	
