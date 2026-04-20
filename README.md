# TODO

### **Run command (Windows)**
```powershell
docker run --rm -v "$($PWD.Path)\test:/data" cityloder-app /data/matera.shp /data/matera_street.geojson /data/matera.las /data/output/matera
```
```
docker run --user $(id -u):$(id -g) ...
```
## Output

After the process is completed, the output folder contains 4 files:
* A mesh containing all buildings (`buildings_mesh.off`)
* A mesh of the ground (`ground_mesh.off`)
* A mesh of the city (`city_mesh.off`)
* A CityJSON model (`city_JSON.city.json`)

## Reference - How to cite our work
This work was presented during STAG2025, with a paper by the title _LiD2LOD: Generating LOD1 Urban Models from Airborne LiDAR_. 
You can access the publication through the Eurographics Digital Library:

* [**DIGLIB proceedings page**](https://diglib.eg.org/items/85255f56-244f-4ee9-a38b-27f1fdf20d48)

* [**Direct PDF link**](https://diglib.eg.org/server/api/core/bitstreams/936212f9-a9aa-4b6c-80e2-f7bd8e092ac1/content)

If you use this work in your research, please cite it as follows:

```bibtex
@inproceedings{10.2312:stag.20251325,
  booktitle = {Smart Tools and Applications in Graphics - Eurographics Italian Chapter Conference},
  editor = {Comino Trinidad, Marc and Mancinelli, Claudio and Maggioli, Filippo and Romanengo, Chiara and Cabiddu, Daniela and Giorgi, Daniela},
  title = {{LiD2LOD: Generating LOD1 Urban Models from Airborne LiDAR}},
  author = {Sorgente, Tommaso and Moscoso Thompson, Elia and Romanengo, Chiara},
  year = {2025},
  publisher = {The Eurographics Association},
  ISSN = {2617-4855},
  ISBN = {978-3-03868-296-7},
  DOI = {10.2312/stag.20251325}
}
```


## Contacts
We appreciate any kind of feedback! Please reach us at:
tommaso.sorgente@cnr.it, chiara.romanengo@cnr.it, elia.moscosothompson@cnr.it
