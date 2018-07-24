//
// Created by Amrik.Sadhra on 20/06/2018.
//

#include "nfs2_loader.h"

void DumpToObj(int block_Idx, PS1::GEO::BLOCK_HEADER *geoBlockHeader, PS1::GEO::BLOCK_3D *vertices,
               PS1::GEO::BLOCK_3D *normals, PS1::GEO::POLY_3D *polygons) {
    std::ofstream obj_dump;
    std::stringstream obj_name;
    obj_name << "./assets/ps1_geo_" << block_Idx << ".obj";
    obj_dump.open(obj_name.str());

    /* Print Part name*/
    obj_dump << "o " << "PS1_Test" << std::endl;
    for (int i = 0; i < geoBlockHeader->nVerts; ++i) {
        obj_dump << "v " << vertices[i].x << " " << vertices[i].y << " " << vertices[i].z << std::endl;
    }

    // TODO: How can these be normals if there aren't enough for Per vertex? On PSX they're likely per polygon
    //for(int i = 0; i <  geoBlockHeader->nNormals; ++i ){
    //    obj_dump << "vn " << normals[i].x << " " << normals[i].y << " " << normals[i].z << std::endl;
    //}

    for (int i = 0; i < geoBlockHeader->nPolygons; ++i) {
        if ((polygons[i].vertex[1][0] == 0) && (polygons[i].vertex[1][1] == 0)) {
            obj_dump << "f " << polygons[i].vertex[0][0] + 1 << " " << polygons[i].vertex[0][1] + 1 << " "
                     << polygons[i].vertex[0][2] + 1 << " " << polygons[i].vertex[0][3] + 1 << std::endl;
        } else if ((polygons[i].vertex[1][0] == 1) && (polygons[i].vertex[1][1] == 1)) {
            // Transparent ?
            obj_dump << "f " << polygons[i].vertex[0][0] + 1 << " " << polygons[i].vertex[0][1] + 1 << " "
                     << polygons[i].vertex[0][2] + 1 << " " << polygons[i].vertex[0][3] + 1 << std::endl;
        } else {
            asm(nyop);
        }
    }
    obj_dump.close();
}

// TODO: Use template specialization/overload to avoid this
template<typename Platform>
std::vector<CarModel> NFS2<Platform>::LoadGEO(const std::string &geo_path) {
    if(std::is_same<Platform,PS1>::value){
        std::cout << "- Parsing GEO File " << std::endl;
        std::vector<CarModel> car_meshes;

        ifstream geo(geo_path, ios::in | ios::binary);

        std::string psh_path = geo_path;
        psh_path.replace(psh_path.find("GEO"), 3, "PSH");

        Utils::ExtractPSH(psh_path, "./assets/car/psx_test/");

        auto *geoFileHeader = new PS1::GEO::HEADER();
        if (geo.read((char *) geoFileHeader, sizeof(PS1::GEO::HEADER)).gcount() != sizeof(PS1::GEO::HEADER)) {
            std::cout << "Couldn't open file/truncated." << std::endl;
            delete geoFileHeader;
            return car_meshes;
        }
        bool eof = false;

        for (int block_Idx = 0;; ++block_Idx) {
            streamoff start = geo.tellg();
            std::cout << block_Idx << " BlockStartOffset: " << start << std::endl;

            auto *geoBlockHeader = new PS1::GEO::BLOCK_HEADER();
            while (geoBlockHeader->nVerts == 0) {
                geo.read((char *) geoBlockHeader, sizeof(PS1::GEO::BLOCK_HEADER));
                if (geo.eof()) {
                    eof = true;
                    break;
                }
            }
            if (eof) break;

            if ((geoBlockHeader->unknown[0] != 0) || (geoBlockHeader->unknown[1] != 1) ||
                (geoBlockHeader->unknown[2] != 1)) {
                std::cout << "Invalid geometry header. This file is special (or corrupt)" << std::endl;
                delete geoBlockHeader;
                delete geoFileHeader;
                return car_meshes;
            }

            auto *vertices = new PS1::GEO::BLOCK_3D[geoBlockHeader->nVerts];
            geo.read((char *) vertices, (geoBlockHeader->nVerts) * sizeof(PS1::GEO::BLOCK_3D));

            std::cout << "VertTblEndOffset: " << geo.tellg() << " Size: " << geo.tellg() - start << std::endl;

            auto *normals = new PS1::GEO::BLOCK_3D[geoBlockHeader->nNormals];
            geo.read((char *) normals, (geoBlockHeader->nNormals) * sizeof(PS1::GEO::BLOCK_3D));

            std::cout << "NormTblEndOffset: " << geo.tellg() << " Size: " << geo.tellg() - start << std::endl;

            auto *xblock_1 = new PS1::GEO::XBLOCK_1();
            auto *xblock_2 = new PS1::GEO::XBLOCK_2();
            auto *xblock_3 = new PS1::GEO::XBLOCK_3();
            auto *xblock_4 = new PS1::GEO::XBLOCK_4();
            auto *xblock_5 = new PS1::GEO::XBLOCK_5();
            switch (geoBlockHeader->unknown1) {
                case 1:
                    geo.read((char *) xblock_1, sizeof(PS1::GEO::XBLOCK_1));
                    break;
                case 2:
                    geo.read((char *) xblock_2, sizeof(PS1::GEO::XBLOCK_2));
                    break;
                case 3:
                    geo.read((char *) xblock_3, sizeof(PS1::GEO::XBLOCK_3));
                    break;
                case 4:
                    geo.read((char *) xblock_4, sizeof(PS1::GEO::XBLOCK_4));
                    break;
                case 5:
                    geo.read((char *) xblock_5, sizeof(PS1::GEO::XBLOCK_5));
                    break;
                default:
                    std::cout << "Unknown block type:  " << geoBlockHeader->unknown1 << std::endl;
            }

            streamoff end = geo.tellg();
            std::cout << "PolyTblStartOffset: " << end << " Size: " << end - start << std::endl;
            // Polygon Table start is aligned on 4 Byte boundary
            if (((end - start) % 4)) {
                std::cout << "Pad Contents: " << std::endl;
                uint16_t *pad = new uint16_t[3];
                geo.read((char *) pad, sizeof(uint16_t) * 3);
                for (int i = 0; i < 3; ++i) {
                    std::cout << pad[i] << std::endl;
                }
                delete[] pad;
            }

            // TODO: This skip is only applicable to DIAB/CORV/others, not F355 or F550 or ARMY. Debug before commit.
            if (block_Idx == 5 || block_Idx == 6) {
                std::cout << "Skipping 12 Bytes - BUT WHY" << std::endl;
                geo.seekg(0xC, ios_base::cur);
            }

            auto *polygons = new PS1::GEO::POLY_3D[geoBlockHeader->nPolygons];
            geo.read((char *) polygons, geoBlockHeader->nPolygons * sizeof(PS1::GEO::POLY_3D));
            DumpToObj(block_Idx, geoBlockHeader, vertices, normals, polygons);

            std::cout << "BlockEndOffset: " << geo.tellg() << " Size: " << geo.tellg() - start << std::endl;

            // Dump GeoBlock data for correlating with geometry/LOD's/Special Cases
            std::cout << "nVerts: " << geoBlockHeader->nVerts << std::endl;
            std::cout << "unknown1: " << geoBlockHeader->unknown1 << std::endl;
            std::cout << "nNormals: " << geoBlockHeader->nNormals << std::endl;
            std::cout << "nPolygons: " << geoBlockHeader->nPolygons << std::endl;
            for (int o = 0; o < 4; ++o) {
                std::cout << geoBlockHeader->unknown2[o][0] << std::endl;
                std::cout << geoBlockHeader->unknown2[o][1] << std::endl;
            }
            for (int j = 0; j < 4; ++j) {
                std::cout << geoBlockHeader->unknown[j] << std::endl;
            }
            switch (geoBlockHeader->unknown1) {
                case 3:
                    std::cout << "XBlock 3: " << std::endl;
                    for (int i = 0; i < sizeof(xblock_3->unknown) / sizeof(xblock_3->unknown[0]); ++i) {
                        std::cout << (int) xblock_3->unknown[i] << std::endl;
                    }
                    break;
                case 1:
                    std::cout << "XBlock 1: " << std::endl;
                    for (int i = 0; i < sizeof(xblock_1->unknown) / sizeof(xblock_1->unknown[0]); ++i) {
                        std::cout << (int) xblock_1->unknown[i] << std::endl;
                    }
                    break;
                case 2:
                    std::cout << "XBlock 2: " << std::endl;
                    for (int i = 0; i < sizeof(xblock_2->unknown) / sizeof(xblock_2->unknown[0]); ++i) {
                        std::cout << (int) xblock_2->unknown[i] << std::endl;
                    }
                    break;
            }
            std::cout << "--------------------------" << std::endl;

            delete geoBlockHeader;
            delete[] normals;
            delete[] vertices;
            delete[] polygons;
            delete xblock_1;
            delete xblock_2;
            delete xblock_3;
            delete xblock_4;
            delete xblock_5;
        }
        delete geoFileHeader;

        return car_meshes;
    } else {
        std::cout << "- Parsing GEO File " << std::endl;
        std::vector<CarModel> car_meshes;

        ifstream geo(geo_path, ios::in | ios::binary);

        auto *geoFileHeader = new PC::GEO::HEADER();
        if (geo.read((char *) geoFileHeader, sizeof(PC::GEO::HEADER)).gcount() != sizeof(PC::GEO::HEADER)) {
            std::cout << "Couldn't open file/truncated." << std::endl;
            delete geoFileHeader;
            return car_meshes;
        }

        while (geo.tellg() != -1) {
            auto *geoBlockHeader = new PC::GEO::BLOCK_HEADER();
            while (geoBlockHeader->nVerts == 0) {
                geo.read((char *) geoBlockHeader, sizeof(PC::GEO::BLOCK_HEADER));
            }

            auto *vertices = new PC::GEO::BLOCK_3D[geoBlockHeader->nVerts];
            geo.read((char *) vertices, geoBlockHeader->nVerts * sizeof(PC::GEO::BLOCK_3D));

            auto *polygons = new PC::GEO::POLY_3D[geoBlockHeader->nPolygons];
            geo.read((char *) polygons, geoBlockHeader->nPolygons * sizeof(PC::GEO::POLY_3D));


            delete geoBlockHeader;
            delete[] vertices;
            delete[] polygons;
        }
        delete geoFileHeader;

        return car_meshes;
    }
}

template<typename Platform>
std::shared_ptr<Car> NFS2<Platform>::LoadCar(const std::string &car_base_path) {
    boost::filesystem::path p(car_base_path);
    std::string car_name = p.filename().string();
    ASSERT(false, "Unimplemented! No UVs or Normals.");
    return std::make_shared<Car>(LoadGEO(car_base_path), NFS_2, car_name);
}
// TRACK
template<typename Platform>
shared_ptr<typename Platform::TRACK> NFS2<Platform>::LoadTrack(const std::string &track_base_path) {
    std::cout << "--- Loading NFS2 Track ---" << std::endl;
    shared_ptr<typename Platform::TRACK> track(new typename Platform::TRACK());

    boost::filesystem::path p(track_base_path);
    std::string track_name = p.filename().string();
    stringstream trk_path, col_path;

    trk_path << track_base_path << ".TRK";
    col_path << track_base_path << ".COL";

    NFSVer nfs_version = UNKNOWN;

    if (std::is_same<Platform, PC>::value) {
        if (track_base_path.find("NFS2_SE") != std::string::npos) {
            nfs_version = NFS_2_SE;
        } else {
            nfs_version = NFS_2;
        }
    } else if (std::is_same<Platform, PS1>::value) {
        nfs_version = NFS_3_PS1;
        std::string ps1_col_path = col_path.str();
        ps1_col_path.replace(ps1_col_path.find("ZZ"), 2, "");
        col_path.str(std::string());
        col_path << ps1_col_path;
    }

    ASSERT(LoadTRK(trk_path.str(), track),
           "Could not load TRK file: " << trk_path.str()); // Load TRK file to get track block specific data
    ASSERT(LoadCOL(col_path.str(), track), "Could not load COL file: "
            << col_path.str()); // Load Catalogue file to get global (non trkblock specific) data
    ASSERT(TrackUtils::ExtractTrackTextures(track_base_path, track_name, nfs_version),
           "Could not extract " << track_name << " texture pack.");

    // Load up the textures
    for (uint32_t tex_Idx = 0; tex_Idx < track->nTextures; tex_Idx++) {
        track->textures[track->polyToQFStexTable[tex_Idx].texNumber] = LoadTexture(track->polyToQFStexTable[tex_Idx],
                                                                                   track_name, nfs_version);
    }
    track->texture_gl_mappings = TrackUtils::GenTrackTextures(track->textures);

    ParseTRKModels(track);
    //std::vector<Track> col_models = ParseCOLModels(track);
    //track->track_blocks[0].objects.insert(track->track_blocks[0].objects.end(), col_models.begin(), col_models.end()); // Insert the COL models into track block 0 for now

    std::cout << "Track loaded successfully" << std::endl;
}

template<typename Platform>
bool NFS2<Platform>::LoadTRK(std::string trk_path, shared_ptr<typename Platform::TRACK> track) {
    std::cout << "- Parsing TRK File " << std::endl;
    ifstream trk(trk_path, ios::in | ios::binary);
    // TRK file header data
    unsigned char header[4];
    long unknownHeader[5];

    // Check we're in a valid TRK file
    if (trk.read(((char *) header), sizeof(unsigned char) * 4).gcount() != sizeof(unsigned char) * 4) {
        std::cout << "Couldn't open file/truncated." << std::endl;
        return false;
    }
    // Header should contain TRAC
    if (memcmp(header, "TRAC", sizeof(header)) != 0) {
        std::cout << "Invalid TRK Header." << std::endl;
        return false;
    }

    // Unknown header data
    if (trk.read(((char *) unknownHeader), sizeof(uint32_t) * 5).gcount() != sizeof(uint32_t) * 5) return false;

    // Basic Track data
    trk.read((char *) &track->nSuperBlocks, sizeof(uint32_t));
    trk.read((char *) &track->nBlocks, sizeof(uint32_t));
    track->superblocks = static_cast<typename Platform::SUPERBLOCK *>(calloc(track->nBlocks,
                                                                             sizeof(typename Platform::SUPERBLOCK)));

    // Offsets of Superblocks in TRK file
    uint32_t *superblockOffsets = static_cast<uint32_t *>(calloc(track->nSuperBlocks, sizeof(uint32_t)));
    if (trk.read(((char *) superblockOffsets), track->nSuperBlocks * sizeof(uint32_t)).gcount() !=
        track->nSuperBlocks * sizeof(uint32_t)) {
        free(superblockOffsets);
        return false;
    }

    // Reference coordinates for each block
    track->blockReferenceCoords = static_cast<VERT_HIGHP *>(calloc(track->nBlocks, sizeof(VERT_HIGHP)));
    if (trk.read((char *) track->blockReferenceCoords, track->nBlocks * sizeof(VERT_HIGHP)).gcount() !=
        track->nBlocks * sizeof(VERT_HIGHP)) {
        free(superblockOffsets);
        return false;
    }

    for (int superBlock_Idx = 0; superBlock_Idx < track->nSuperBlocks; ++superBlock_Idx) {
        std::cout << "SuperBlock " << superBlock_Idx + 1 << " of " << track->nSuperBlocks << std::endl;
        // Get the superblock header
        auto *superblock = &track->superblocks[superBlock_Idx];
        trk.seekg(superblockOffsets[superBlock_Idx], ios_base::beg);
        trk.read((char *) &superblock->superBlockSize, sizeof(uint32_t));
        trk.read((char *) &superblock->nBlocks, sizeof(uint32_t));
        trk.read((char *) &superblock->padding, sizeof(uint32_t));

        if (superblock->nBlocks != 0) {
            // Get the offsets of the child blocks within superblock
            uint32_t *blockOffsets = (uint32_t *) calloc(static_cast<size_t>(superblock->nBlocks), sizeof(uint32_t));
            trk.read((char *) blockOffsets, superblock->nBlocks * sizeof(uint32_t));
            superblock->trackBlocks = static_cast<typename Platform::TRKBLOCK *>(calloc(
                    static_cast<size_t>(superblock->nBlocks), sizeof(typename Platform::TRKBLOCK)));

            for (int block_Idx = 0; block_Idx < superblock->nBlocks; ++block_Idx) {
                std::cout << "  Block " << block_Idx + 1 << " of " << superblock->nBlocks << std::endl;
                auto *trackblock = &superblock->trackBlocks[block_Idx];
                // Read Header
                trackblock->header = static_cast<TRKBLOCK_HEADER *>(calloc(1, sizeof(TRKBLOCK_HEADER)));
                trk.seekg(superblockOffsets[superBlock_Idx] + blockOffsets[block_Idx], ios_base::beg);
                trk.read((char *) trackblock->header, sizeof(TRKBLOCK_HEADER));

                std::cout << trackblock->header->unknownPad[0] << " " << trackblock->header->unknownPad[1] << " "
                          << trackblock->header->unknownPad[2] << std::endl;

                // Sanity Checks
                if ((trackblock->header->blockSize != trackblock->header->blockSizeDup) ||
                    (trackblock->header->blockSerial > track->nBlocks)) {
                    std::cout << "   --- Bad Block" << std::endl;
                    free(superblockOffsets);
                    return false;
                }

                // Read 3D Data
                trackblock->vertexTable = static_cast<typename Platform::VERT *>(calloc(
                        static_cast<size_t>(trackblock->header->nStickToNextVerts + trackblock->header->nHighResVert),
                        sizeof(typename Platform::VERT)));
                for (unsigned int vert_Idx = 0;
                     vert_Idx < trackblock->header->nStickToNextVerts + trackblock->header->nHighResVert; ++vert_Idx) {
                    trk.read((char *) &trackblock->vertexTable[vert_Idx], sizeof(typename Platform::VERT));
                }

                trackblock->polygonTable = static_cast<typename Platform::POLYGONDATA *>(calloc(
                        static_cast<size_t>(trackblock->header->nLowResPoly + trackblock->header->nMedResPoly +
                                            trackblock->header->nHighResPoly), sizeof(typename Platform::POLYGONDATA)));
                for (unsigned int poly_Idx = 0; poly_Idx <
                                                (trackblock->header->nLowResPoly + trackblock->header->nMedResPoly +
                                                 trackblock->header->nHighResPoly); ++poly_Idx) {
                    trk.read((char *) &trackblock->polygonTable[poly_Idx], sizeof(typename Platform::POLYGONDATA));
                }

                // Read Extrablock data
                trk.seekg(superblockOffsets[superBlock_Idx] + blockOffsets[block_Idx] + 64u +
                          trackblock->header->extraBlockTblOffset, ios_base::beg);
                // Get extrablock offsets (relative to beginning of TrackBlock)
                uint32_t *extrablockOffsets = (uint32_t *) calloc(trackblock->header->nExtraBlocks, sizeof(uint32_t));
                trk.read((char *) extrablockOffsets, trackblock->header->nExtraBlocks * sizeof(uint32_t));

                for (int xblock_Idx = 0; xblock_Idx < trackblock->header->nExtraBlocks; ++xblock_Idx) {
                    trk.seekg(
                            superblockOffsets[superBlock_Idx] + blockOffsets[block_Idx] + extrablockOffsets[xblock_Idx],
                            ios_base::beg);
                    auto *xblockHeader = static_cast<EXTRABLOCK_HEADER *>(calloc(1, sizeof(EXTRABLOCK_HEADER)));
                    trk.read((char *) xblockHeader, sizeof(EXTRABLOCK_HEADER));

                    switch (xblockHeader->XBID) {
                        case 5:
                            trackblock->polyTypes = static_cast<POLY_TYPE *>(calloc(xblockHeader->nRecords,
                                                                                    sizeof(POLY_TYPE)));
                            trk.read((char *) trackblock->polyTypes, xblockHeader->nRecords * sizeof(POLY_TYPE));
                            break;
                        case 4:
                            trackblock->nNeighbours = xblockHeader->nRecords;
                            trackblock->blockNeighbours = (uint16_t *) calloc(xblockHeader->nRecords, sizeof(uint16_t));
                            trk.read((char *) trackblock->blockNeighbours, xblockHeader->nRecords * sizeof(uint16_t));
                            break;
                        case 8:
                            trackblock->structures = static_cast<typename Platform::GEOM_BLOCK *>(calloc(
                                    xblockHeader->nRecords, sizeof(typename Platform::GEOM_BLOCK)));
                            trackblock->nStructures = xblockHeader->nRecords;
                            for (int structure_Idx = 0; structure_Idx < trackblock->nStructures; ++structure_Idx) {
                                streamoff padCheck = trk.tellg();
                                trk.read((char *) &trackblock->structures[structure_Idx].recSize, sizeof(uint32_t));
                                trk.read((char *) &trackblock->structures[structure_Idx].nVerts, sizeof(uint16_t));
                                trk.read((char *) &trackblock->structures[structure_Idx].nPoly, sizeof(uint16_t));

                                trackblock->structures[structure_Idx].vertexTable = static_cast<typename Platform::VERT *>(calloc(
                                        trackblock->structures[structure_Idx].nVerts, sizeof(typename Platform::VERT)));
                                for (int vert_Idx = 0;
                                     vert_Idx < trackblock->structures[structure_Idx].nVerts; ++vert_Idx) {
                                    trk.read((char *) &trackblock->structures[structure_Idx].vertexTable[vert_Idx],
                                             sizeof(typename Platform::VERT));
                                }
                                trackblock->structures[structure_Idx].polygonTable = static_cast<typename Platform::POLYGONDATA *>(calloc(
                                        trackblock->structures[structure_Idx].nPoly,
                                        sizeof(typename Platform::POLYGONDATA)));
                                for (int poly_Idx = 0;
                                     poly_Idx < trackblock->structures[structure_Idx].nPoly; ++poly_Idx) {
                                    trk.read((char *) &trackblock->structures[structure_Idx].polygonTable[poly_Idx],
                                             sizeof(typename Platform::POLYGONDATA));
                                }
                                trk.seekg(trackblock->structures[structure_Idx].recSize - (trk.tellg() - padCheck),
                                          ios_base::cur); // Eat possible padding
                            }
                            break;
                        case 7:
                        case 18:
                            trackblock->structureRefData = static_cast<GEOM_REF_BLOCK *>(calloc(xblockHeader->nRecords,
                                                                                                sizeof(GEOM_REF_BLOCK)));
                            trackblock->nStructureReferences = xblockHeader->nRecords;
                            for (int structureRef_Idx = 0;
                                 structureRef_Idx < trackblock->nStructureReferences; ++structureRef_Idx) {
                                streamoff padCheck = trk.tellg();
                                trk.read((char *) &trackblock->structureRefData[structureRef_Idx].recSize,
                                         sizeof(uint16_t));
                                trk.read((char *) &trackblock->structureRefData[structureRef_Idx].recType,
                                         sizeof(uint8_t));
                                trk.read((char *) &trackblock->structureRefData[structureRef_Idx].structureRef,
                                         sizeof(uint8_t));
                                // Fixed type
                                if (trackblock->structureRefData[structureRef_Idx].recType == 1) {
                                    trk.read((char *) &trackblock->structureRefData[structureRef_Idx].refCoordinates,
                                             sizeof(VERT_HIGHP));
                                } else if (trackblock->structureRefData[structureRef_Idx].recType ==
                                           3) { // Animated type
                                    trk.read((char *) &trackblock->structureRefData[structureRef_Idx].animLength,
                                             sizeof(uint16_t));
                                    trk.read((char *) &trackblock->structureRefData[structureRef_Idx].unknown,
                                             sizeof(uint16_t));
                                    trackblock->structureRefData[structureRef_Idx].animationData = static_cast<ANIM_POS *>(calloc(
                                            trackblock->structureRefData[structureRef_Idx].animLength,
                                            sizeof(ANIM_POS)));
                                    for (int animation_Idx = 0; animation_Idx <
                                                                trackblock->structureRefData[structureRef_Idx].animLength; ++animation_Idx) {
                                        trk.read(
                                                (char *) &trackblock->structureRefData[structureRef_Idx].animationData[animation_Idx],
                                                sizeof(ANIM_POS));
                                    }
                                } else if (trackblock->structureRefData[structureRef_Idx].recType == 4) {
                                    // 4 Component PSX Vert data? TODO: Restructure to allow the 4th component to be read
                                    trk.read((char *) &trackblock->structureRefData[structureRef_Idx].refCoordinates,
                                             sizeof(VERT_HIGHP));
                                } else {
                                    std::cout << "Unknown Structure Reference type: "
                                              << (int) trackblock->structureRefData[structureRef_Idx].recType
                                              << " Size: "
                                              << (int) trackblock->structureRefData[structureRef_Idx].recSize
                                              << " StructRef: "
                                              << (int) trackblock->structureRefData[structureRef_Idx].structureRef
                                              << std::endl;
                                }
                                trk.seekg(trackblock->structureRefData[structureRef_Idx].recSize -
                                          (trk.tellg() - padCheck), ios_base::cur); // Eat possible padding
                            }
                            break;
                        case 6:
                            trackblock->medianData = static_cast<MEDIAN_BLOCK *>(calloc(xblockHeader->nRecords,
                                                                                        sizeof(MEDIAN_BLOCK)));
                            trk.read((char *) trackblock->medianData, xblockHeader->nRecords * sizeof(MEDIAN_BLOCK));
                            break;
                        case 13:
                            trackblock->nVroad = xblockHeader->nRecords;
                            trackblock->vroadData = static_cast<typename Platform::VROAD *>(calloc(
                                    xblockHeader->nRecords, sizeof(typename Platform::VROAD)));
                            trk.read((char *) trackblock->vroadData,
                                     trackblock->nVroad * sizeof(typename Platform::VROAD));
                            break;
                        case 9:
                            trackblock->nLanes = xblockHeader->nRecords;
                            trackblock->laneData = static_cast<LANE_BLOCK *>(calloc(xblockHeader->nRecords,
                                                                                    sizeof(LANE_BLOCK)));
                            trk.read((char *) trackblock->laneData, trackblock->nLanes * sizeof(LANE_BLOCK));
                            break;
                        default:
                            std::cout << "Unknown XBID: " << xblockHeader->XBID << " nRecords: "
                                      << xblockHeader->nRecords << " RecSize: " << xblockHeader->recSize << std::endl;
                            break;
                    }
                    free(xblockHeader);
                }
                free(extrablockOffsets);
            }
            free(blockOffsets);
        }
    }
    free(superblockOffsets);
    trk.close();
    return true;
}

template<typename Platform>
bool NFS2<Platform>::LoadCOL(std::string col_path, shared_ptr<typename Platform::TRACK> track) {
    std::cout << "- Parsing COL File " << std::endl;
    ifstream col(col_path, ios::in | ios::binary);
    // Check we're in a valid TRK file
    unsigned char header[4];
    if (col.read(((char *) header), sizeof(unsigned char) * 4).gcount() != sizeof(unsigned char) * 4) return false;
    if (memcmp(header, "COLL", sizeof(header)) != 0) return false;

    uint32_t version;
    col.read((char *) &version, sizeof(uint32_t));
    if (version != 11) return false;

    uint32_t colSize;
    col.read((char *) &colSize, sizeof(uint32_t));

    uint32_t nExtraBlocks;
    col.read((char *) &nExtraBlocks, sizeof(uint32_t));

    uint32_t *extraBlockOffsets = (uint32_t *) calloc(nExtraBlocks, sizeof(uint32_t));
    col.read((char *) extraBlockOffsets, nExtraBlocks * sizeof(uint32_t));

    std::cout << "  Version: " << version << "\n  nExtraBlocks: " << nExtraBlocks << "\nParsing COL Extrablocks"
              << std::endl;

    for (int xBlock_Idx = 0; xBlock_Idx < nExtraBlocks; ++xBlock_Idx) {
        col.seekg(16 + extraBlockOffsets[xBlock_Idx], ios_base::beg);

        auto *xblockHeader = static_cast<EXTRABLOCK_HEADER *>(calloc(1, sizeof(EXTRABLOCK_HEADER)));
        col.read((char *) xblockHeader, sizeof(EXTRABLOCK_HEADER));

        std::cout << "  XBID " << (int) xblockHeader->XBID << " (XBlock " << xBlock_Idx + 1 << " of " << nExtraBlocks
                  << ")" << std::endl;

        switch (xblockHeader->XBID) {
            case 2: // First xbock always texture table
                track->nTextures = xblockHeader->nRecords;
                track->polyToQFStexTable = static_cast<TEXTURE_BLOCK *>(calloc(track->nTextures,
                                                                               sizeof(TEXTURE_BLOCK)));
                col.read((char *) track->polyToQFStexTable, track->nTextures * sizeof(TEXTURE_BLOCK));
                break;
            case 8: // XBID 8 3D Structure data: This block is only present if nExtraBlocks != 2
                track->nColStructures = xblockHeader->nRecords;
                track->colStructures = static_cast<typename Platform::GEOM_BLOCK *>(calloc(track->nColStructures,
                                                                                           sizeof(typename Platform::GEOM_BLOCK)));
                for (int structure_Idx = 0; structure_Idx < track->nColStructures; ++structure_Idx) {
                    streamoff padCheck = col.tellg();
                    col.read((char *) &track->colStructures[structure_Idx].recSize, sizeof(uint32_t));
                    col.read((char *) &track->colStructures[structure_Idx].nVerts, sizeof(uint16_t));
                    col.read((char *) &track->colStructures[structure_Idx].nPoly, sizeof(uint16_t));
                    track->colStructures[structure_Idx].vertexTable = static_cast<typename Platform::VERT *>(calloc(
                            track->colStructures[structure_Idx].nVerts, sizeof(typename Platform::VERT)));
                    for (int vert_Idx = 0; vert_Idx < track->colStructures[structure_Idx].nVerts; ++vert_Idx) {
                        col.read((char *) &track->colStructures[structure_Idx].vertexTable[vert_Idx],
                                 sizeof(typename Platform::VERT));
                    }
                    track->colStructures[structure_Idx].polygonTable = static_cast<typename Platform::POLYGONDATA *>(calloc(
                            track->colStructures[structure_Idx].nPoly, sizeof(typename Platform::POLYGONDATA)));
                    for (int poly_Idx = 0; poly_Idx < track->colStructures[structure_Idx].nPoly; ++poly_Idx) {
                        col.read((char *) &track->colStructures[structure_Idx].polygonTable[poly_Idx],
                                 sizeof(typename Platform::POLYGONDATA));
                    }
                    col.seekg(track->colStructures[structure_Idx].recSize - (col.tellg() - padCheck),
                              ios_base::cur); // Eat possible padding
                }
                break;
            case 7: // XBID 7 3D Structure Reference: This block is only present if nExtraBlocks != 2
                track->nColStructureReferences = xblockHeader->nRecords;
                track->colStructureRefData = static_cast<GEOM_REF_BLOCK *>(calloc(track->nColStructureReferences,
                                                                                  sizeof(GEOM_REF_BLOCK)));
                for (int structureRef_Idx = 0; structureRef_Idx < track->nColStructures; ++structureRef_Idx) {
                    streamoff padCheck = col.tellg();
                    col.read((char *) &track->colStructureRefData[structureRef_Idx].recSize, sizeof(uint16_t));
                    col.read((char *) &track->colStructureRefData[structureRef_Idx].recType, sizeof(uint8_t));
                    col.read((char *) &track->colStructureRefData[structureRef_Idx].structureRef, sizeof(uint8_t));
                    // Fixed type
                    if (track->colStructureRefData[structureRef_Idx].recType == 1) {
                        col.read((char *) &track->colStructureRefData[structureRef_Idx].refCoordinates,
                                 sizeof(VERT_HIGHP));
                    } else if (track->colStructureRefData[structureRef_Idx].recType == 3) { // Animated type
                        col.read((char *) &track->colStructureRefData[structureRef_Idx].animLength, sizeof(uint16_t));
                        col.read((char *) &track->colStructureRefData[structureRef_Idx].unknown, sizeof(uint16_t));
                        track->colStructureRefData[structureRef_Idx].animationData = static_cast<ANIM_POS *>(calloc(
                                track->colStructureRefData[structureRef_Idx].animLength, sizeof(ANIM_POS)));
                        for (int animation_Idx = 0;
                             animation_Idx < track->colStructureRefData[structureRef_Idx].animLength; ++animation_Idx) {
                            col.read(
                                    (char *) &track->colStructureRefData[structureRef_Idx].animationData[animation_Idx],
                                    sizeof(ANIM_POS));
                        }
                    } else if (track->colStructureRefData[structureRef_Idx].recType == 4) {
                        // 4 Component PSX Vert data? TODO: Restructure to allow the 4th component to be read
                        col.read((char *) &track->colStructureRefData[structureRef_Idx].refCoordinates,
                                 sizeof(VERT_HIGHP));
                    } else {
                        std::cout << "Unknown Structure Reference type: "
                                  << (int) track->colStructureRefData[structureRef_Idx].recType << std::endl;
                    }
                    col.seekg(track->colStructureRefData[structureRef_Idx].recSize - (col.tellg() - padCheck),
                              ios_base::cur); // Eat possible padding
                }
                break;
            case 15:
                track->nCollisionData = xblockHeader->nRecords;
                track->collisionData = static_cast<COLLISION_BLOCK *>(calloc(track->nCollisionData,
                                                                             sizeof(COLLISION_BLOCK)));
                col.read((char *) track->collisionData, track->nCollisionData * sizeof(COLLISION_BLOCK));
                break;
            default:
                break;
        }
        free(xblockHeader);
    }
    col.close();
    return true;
}

template<typename Platform>
void NFS2<Platform>::dbgPrintVerts(const std::string &path, shared_ptr<typename Platform::TRACK> track) {
    std::ofstream obj_dump;

    if (!(boost::filesystem::exists(path))) {
        boost::filesystem::create_directories(path);
    }

    // Parse out TRKBlock data
    for (int superBlock_Idx = 0; superBlock_Idx < track->nSuperBlocks; ++superBlock_Idx) {
        auto *superblock = &track->superblocks[superBlock_Idx];
        for (int block_Idx = 0; block_Idx < superblock->nBlocks; ++block_Idx) {
            auto trkBlock = superblock->trackBlocks[block_Idx];
            VERT_HIGHP blockReferenceCoord;
            // Print clipping rectangle
            //obj_dump << "o Block" << trkBlock.header->blockSerial << "ClippingRect" << std::endl;
            //for(int i = 0; i < 4; i++){
            //    obj_dump << "v " << trkBlock.header->clippingRect[i].x << " " << trkBlock.header->clippingRect[i].z << " " << trkBlock.header->clippingRect[i].y << std::endl;
            //}
            // obj_dump << "f " << 1+(4*trkBlock.header->blockSerial) << " " << 2+(4*trkBlock.header->blockSerial) << " " << 3+(4*trkBlock.header->blockSerial) << " " << 4+(4*trkBlock.header->blockSerial) << std::endl;
            std::ostringstream stringStream;
            stringStream << path << "TrackBlock" << trkBlock.header->blockSerial << ".obj";
            obj_dump.open(stringStream.str());
            obj_dump << "o TrackBlock" << trkBlock.header->blockSerial << std::endl;
            for (int i = 0; i < trkBlock.header->nStickToNextVerts + trkBlock.header->nHighResVert; i++) {
                if (i < trkBlock.header->nStickToNextVerts) {
                    // If in last block go get ref coord of first block, else get ref of next block
                    blockReferenceCoord = (trkBlock.header->blockSerial == track->nBlocks - 1)
                                          ? track->blockReferenceCoords[0] : track->blockReferenceCoords[
                                                  trkBlock.header->blockSerial + 1];
                } else {
                    blockReferenceCoord = track->blockReferenceCoords[trkBlock.header->blockSerial];
                }
                int32_t x = (blockReferenceCoord.x + (256 * trkBlock.vertexTable[i].x));
                int32_t y = (blockReferenceCoord.y + (256 * trkBlock.vertexTable[i].y));
                int32_t z = (blockReferenceCoord.z + (256 * trkBlock.vertexTable[i].z));
                obj_dump << "v " << x / scaleFactor << " " << z / scaleFactor << " " << y / scaleFactor << std::endl;
            }
            for (int poly_Idx = (trkBlock.header->nLowResPoly + trkBlock.header->nMedResPoly); poly_Idx <
                                                                                               (trkBlock.header->nLowResPoly +
                                                                                                trkBlock.header->nMedResPoly +
                                                                                                trkBlock.header->nHighResPoly); ++poly_Idx) {
                obj_dump << "f " << (unsigned int) trkBlock.polygonTable[poly_Idx].vertex[0] + 1 << " "
                         << (unsigned int) trkBlock.polygonTable[poly_Idx].vertex[1] + 1 << " "
                         << (unsigned int) trkBlock.polygonTable[poly_Idx].vertex[2] + 1 << " "
                         << (unsigned int) trkBlock.polygonTable[poly_Idx].vertex[3] + 1 << std::endl;
            }
            obj_dump.close();
            for (int structure_Idx = 0; structure_Idx < trkBlock.nStructures; ++structure_Idx) {
                std::ostringstream stringStream1;
                stringStream1 << path << "SB" << superBlock_Idx << "TB" << block_Idx << "S" << structure_Idx << ".obj";
                obj_dump.open(stringStream1.str());
                VERT_HIGHP *structureReferenceCoordinates = &track->blockReferenceCoords[trkBlock.header->blockSerial];
                // Find the structure reference that matches this structure, else use block default
                for (int structRef_Idx = 0; structRef_Idx < trkBlock.nStructureReferences; ++structRef_Idx) {
                    // Only check fixed type structure references
                    if (trkBlock.structureRefData[structRef_Idx].structureRef == structure_Idx) {
                        if (trkBlock.structureRefData[structRef_Idx].recType == 1 ||
                            trkBlock.structureRefData[structRef_Idx].recType == 4) {
                            structureReferenceCoordinates = &trkBlock.structureRefData[structure_Idx].refCoordinates;
                            break;
                        } else if (trkBlock.structureRefData[structRef_Idx].recType == 3) {
                            if (trkBlock.structureRefData[structure_Idx].animLength != 0) {
                                // For now, if animated, use position 0 of animation sequence
                                structureReferenceCoordinates = &trkBlock.structureRefData[structure_Idx].animationData[0].position;
                                break;
                            }
                        }
                    }
                    if (structRef_Idx == trkBlock.nStructureReferences - 1)
                        std::cout << "Couldn't find a reference coordinate for Structure " << structRef_Idx << " in SB"
                                  << superBlock_Idx << "TB" << block_Idx << std::endl;
                }
                obj_dump << "o Struct" << &trkBlock.structures[structure_Idx] << std::endl;
                for (uint16_t vert_Idx = 0; vert_Idx < trkBlock.structures[structure_Idx].nVerts; ++vert_Idx) {
                    int32_t x = (structureReferenceCoordinates->x +
                                 (256 * trkBlock.structures[structure_Idx].vertexTable[vert_Idx].x));
                    int32_t y = (structureReferenceCoordinates->y +
                                 (256 * trkBlock.structures[structure_Idx].vertexTable[vert_Idx].y));
                    int32_t z = (structureReferenceCoordinates->z +
                                 (256 * trkBlock.structures[structure_Idx].vertexTable[vert_Idx].z));
                    obj_dump << "v " << x / scaleFactor << " " << z / scaleFactor << " " << y / scaleFactor
                             << std::endl;
                }
                for (int poly_Idx = 0; poly_Idx < trkBlock.structures[structure_Idx].nPoly; ++poly_Idx) {
                    obj_dump << "f "
                             << (unsigned int) trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[0] + 1
                             << " "
                             << (unsigned int) trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[1] + 1
                             << " "
                             << (unsigned int) trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[2] + 1
                             << " "
                             << (unsigned int) trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[3] + 1
                             << std::endl;
                }
                obj_dump.close();
            }
        }
    }

    // Parse out COL data
    for (int structure_Idx = 0; structure_Idx < track->nColStructures; ++structure_Idx) {
        std::ostringstream stringStream1;
        stringStream1 << path << "COL" << structure_Idx << ".obj";
        obj_dump.open(stringStream1.str());
        VERT_HIGHP *structureReferenceCoordinates = static_cast<VERT_HIGHP *>(calloc(1, sizeof(VERT_HIGHP)));
        // Find the structure reference that matches this structure, else use block default
        for (int structRef_Idx = 0; structRef_Idx < track->nColStructureReferences; ++structRef_Idx) {
            // Only check fixed type structure references
            if (track->colStructureRefData[structRef_Idx].structureRef == structure_Idx) {
                if (track->colStructureRefData[structRef_Idx].recType == 1 ||
                    track->colStructureRefData[structRef_Idx].recType == 4) {
                    structureReferenceCoordinates = &track->colStructureRefData[structure_Idx].refCoordinates;
                    break;
                } else if (track->colStructureRefData[structRef_Idx].recType == 3) {
                    if (track->colStructureRefData[structure_Idx].animLength != 0) {
                        // For now, if animated, use position 0 of animation sequence
                        structureReferenceCoordinates = &track->colStructureRefData[structure_Idx].animationData[0].position;
                        break;
                    }
                }
            }
            if (structRef_Idx == track->nColStructureReferences - 1)
                std::cout << "Couldn't find a reference coordinate for COL Structure " << structRef_Idx << std::endl;
        }
        obj_dump << "o ColStruct" << &track->colStructures[structure_Idx] << std::endl;
        for (uint16_t vert_Idx = 0; vert_Idx < track->colStructures[structure_Idx].nVerts; ++vert_Idx) {
            int32_t x = (structureReferenceCoordinates->x +
                         (256 * track->colStructures[structure_Idx].vertexTable[vert_Idx].x));
            int32_t y = (structureReferenceCoordinates->y +
                         (256 * track->colStructures[structure_Idx].vertexTable[vert_Idx].y));
            int32_t z = (structureReferenceCoordinates->z +
                         (256 * track->colStructures[structure_Idx].vertexTable[vert_Idx].z));
            obj_dump << "v " << x / scaleFactor << " " << z / scaleFactor << " " << y / scaleFactor << std::endl;
        }
        for (int poly_Idx = 0; poly_Idx < track->colStructures[structure_Idx].nPoly; ++poly_Idx) {
            obj_dump << "f " << (unsigned int) track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[0] + 1
                     << " " << (unsigned int) track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[1] + 1
                     << " " << (unsigned int) track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[2] + 1
                     << " " << (unsigned int) track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[3] + 1
                     << std::endl;
        }
        obj_dump.close();
        free(structureReferenceCoordinates);
    }
}

template<typename Platform>
void NFS2<Platform>::ParseTRKModels(shared_ptr<typename Platform::TRACK> track) {
    // Parse out TRKBlock data
    for (int superBlock_Idx = 0; superBlock_Idx < track->nSuperBlocks; ++superBlock_Idx) {
        auto *superblock = &track->superblocks[superBlock_Idx];
        for (int block_Idx = 0; block_Idx < superblock->nBlocks; ++block_Idx) {
            // Base Track Geometry
            auto trkBlock = superblock->trackBlocks[block_Idx];
            VERT_HIGHP blockReferenceCoord;

            TrackBlock current_track_block(superBlock_Idx, glm::vec3(trkBlock.header->clippingRect->x / scaleFactor,
                                                                     trkBlock.header->clippingRect->y / scaleFactor,
                                                                     trkBlock.header->clippingRect->z / scaleFactor));
            glm::quat orientation = glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0)));
            glm::vec3 trk_block_center = orientation * glm::vec3(0, 0, 0);
            std::cout << "Trk block " << (int) trkBlock.header->blockSerial << " NStruct: " << trkBlock.nStructures
                      << std::endl;
            // Structures
            for (int structure_Idx = 0; structure_Idx < trkBlock.nStructures; ++structure_Idx) {
                // Keep track of unique textures in trackblock for later OpenGL bind
                std::set<short> minimal_texture_ids_set;
                // Mesh Data
                std::vector<unsigned int> vertex_indices;
                std::vector<glm::vec2> uvs;
                std::vector<unsigned int> texture_indices;
                std::vector<glm::vec3> verts;
                std::vector<glm::vec4> shading_verts;
                std::vector<glm::vec3> norms;

                VERT_HIGHP *structureReferenceCoordinates = &track->blockReferenceCoords[trkBlock.header->blockSerial];
                // Find the structure reference that matches this structure, else use block default
                for (int structRef_Idx = 0; structRef_Idx < trkBlock.nStructureReferences; ++structRef_Idx) {
                    // Only check fixed type structure references
                    if (trkBlock.structureRefData[structRef_Idx].structureRef == structure_Idx) {
                        if (trkBlock.structureRefData[structRef_Idx].recType == 1 ||
                            trkBlock.structureRefData[structRef_Idx].recType == 4) {
                            structureReferenceCoordinates = &trkBlock.structureRefData[structure_Idx].refCoordinates;
                            break;
                        } else if (trkBlock.structureRefData[structRef_Idx].recType == 3) {
                            //if(trkBlock.structureRefData[structure_Idx].animLength != 0){
                            // For now, if animated, use position 0 of animation sequence
                            structureReferenceCoordinates = &trkBlock.structureRefData[structure_Idx].animationData[0].position;
                            break;
                            //}
                        }
                    }
                    //if (structRef_Idx == trkBlock.nStructureReferences - 1){}
                    //    std::cout << "Couldn't find a reference coordinate for Structure " << structRef_Idx << " in SB" << superBlock_Idx << "TB" << block_Idx << std::endl;
                }
                for (uint16_t vert_Idx = 0; vert_Idx < trkBlock.structures[structure_Idx].nVerts; ++vert_Idx) {
                    int32_t x = (structureReferenceCoordinates->x +
                                 (256 * trkBlock.structures[structure_Idx].vertexTable[vert_Idx].x));
                    int32_t y = (structureReferenceCoordinates->y +
                                 (256 * trkBlock.structures[structure_Idx].vertexTable[vert_Idx].y));
                    int32_t z = (structureReferenceCoordinates->z +
                                 (256 * trkBlock.structures[structure_Idx].vertexTable[vert_Idx].z));
                    verts.emplace_back(glm::vec3(x / scaleFactor, y / scaleFactor, z / scaleFactor));
                    shading_verts.emplace_back(glm::vec4(1.0, 1.0f, 1.0f, 1.0f));
                }
                for (int poly_Idx = 0; poly_Idx < trkBlock.structures[structure_Idx].nPoly; ++poly_Idx) {
                    // Remap the COL TextureID's using the COL texture block (XBID2)
                    TEXTURE_BLOCK texture_for_block = track->polyToQFStexTable[trkBlock.structures[structure_Idx].polygonTable[poly_Idx].texture];
                    minimal_texture_ids_set.insert(texture_for_block.texNumber);
                    vertex_indices.emplace_back(trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[0]);
                    vertex_indices.emplace_back(trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[1]);
                    vertex_indices.emplace_back(trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[2]);
                    vertex_indices.emplace_back(trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[0]);
                    vertex_indices.emplace_back(trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[2]);
                    vertex_indices.emplace_back(trkBlock.structures[structure_Idx].polygonTable[poly_Idx].vertex[3]);
                    // TODO: Use textures alignment data to modify these UV's
                    uvs.emplace_back(1.0f, 1.0f);
                    uvs.emplace_back(0.0f, 1.0f);
                    uvs.emplace_back(0.0f, 0.0f);
                    uvs.emplace_back(1.0f, 1.0f);
                    uvs.emplace_back(0.0f, 0.0f);
                    uvs.emplace_back(1.0f, 0.0f);
                    texture_indices.emplace_back(texture_for_block.texNumber);
                    texture_indices.emplace_back(texture_for_block.texNumber);
                    texture_indices.emplace_back(texture_for_block.texNumber);
                    texture_indices.emplace_back(texture_for_block.texNumber);
                    texture_indices.emplace_back(texture_for_block.texNumber);
                    texture_indices.emplace_back(texture_for_block.texNumber);
                    // TODO: Calculate normals properly
                    norms.emplace_back(glm::vec3(1, 1, 1));
                    norms.emplace_back(glm::vec3(1, 1, 1));
                    norms.emplace_back(glm::vec3(1, 1, 1));
                    norms.emplace_back(glm::vec3(1, 1, 1));
                    norms.emplace_back(glm::vec3(1, 1, 1));
                    norms.emplace_back(glm::vec3(1, 1, 1));
                }
                std::stringstream xobj_name;
                xobj_name << "SB" << superBlock_Idx << "TB" << block_Idx << "S" << structure_Idx << ".obj";
                // Get ordered list of unique texture id's present in block
                std::vector<short> texture_ids = TrackUtils::RemapTextureIDs(minimal_texture_ids_set, texture_indices);
                Track xobj_model = Track(xobj_name.str(), trkBlock.header->blockSerial * structure_Idx, verts, norms,
                                         uvs, texture_indices, vertex_indices, texture_ids, shading_verts,
                                         trk_block_center);
                xobj_model.enable();
                current_track_block.objects.emplace_back(xobj_model);
            }

            // Keep track of unique textures in trackblock for later OpenGL bind
            std::set<short> minimal_texture_ids_set;
            // Mesh Data
            std::vector<unsigned int> vertex_indices;
            std::vector<glm::vec2> uvs;
            std::vector<unsigned int> texture_indices;
            std::vector<glm::vec3> verts;
            std::vector<glm::vec4> trk_block_shading_verts;
            std::vector<glm::vec3> norms;

            for (int i = 0; i < trkBlock.header->nStickToNextVerts + trkBlock.header->nHighResVert; i++) {
                if (i < trkBlock.header->nStickToNextVerts) {
                    // If in last block go get ref coord of first block, else get ref of next block
                    blockReferenceCoord = (trkBlock.header->blockSerial == track->nBlocks - 1)
                                          ? track->blockReferenceCoords[0] : track->blockReferenceCoords[
                                                  trkBlock.header->blockSerial + 1];
                } else {
                    blockReferenceCoord = track->blockReferenceCoords[trkBlock.header->blockSerial];
                }
                int32_t x = (blockReferenceCoord.x + (256 * trkBlock.vertexTable[i].x));
                int32_t y = (blockReferenceCoord.y + (256 * trkBlock.vertexTable[i].y));
                int32_t z = (blockReferenceCoord.z + (256 * trkBlock.vertexTable[i].z));
                verts.emplace_back(glm::vec3(x / scaleFactor, y / scaleFactor, z / scaleFactor));
                trk_block_shading_verts.emplace_back(glm::vec4(1.0, 1.0f, 1.0f, 1.0f));

            }
            for (int poly_Idx = (trkBlock.header->nLowResPoly + trkBlock.header->nMedResPoly); poly_Idx <
                                                                                               (trkBlock.header->nLowResPoly +
                                                                                                trkBlock.header->nMedResPoly +
                                                                                                trkBlock.header->nHighResPoly); ++poly_Idx) {
                // Remap the COL TextureID's using the COL texture block (XBID2)
                TEXTURE_BLOCK texture_for_block = track->polyToQFStexTable[trkBlock.polygonTable[poly_Idx].texture];
                minimal_texture_ids_set.insert(texture_for_block.texNumber);
                vertex_indices.emplace_back(trkBlock.polygonTable[poly_Idx].vertex[0]);
                vertex_indices.emplace_back(trkBlock.polygonTable[poly_Idx].vertex[1]);
                vertex_indices.emplace_back(trkBlock.polygonTable[poly_Idx].vertex[2]);
                vertex_indices.emplace_back(trkBlock.polygonTable[poly_Idx].vertex[0]);
                vertex_indices.emplace_back(trkBlock.polygonTable[poly_Idx].vertex[2]);
                vertex_indices.emplace_back(trkBlock.polygonTable[poly_Idx].vertex[3]);

                float scrollA = ((texture_for_block.alignmentData >> 12) & 0xF) / 16.0f;
                float scrollB = ((texture_for_block.alignmentData >> 8) & 0xF) / 16.0f;
                float scrollC = ((texture_for_block.alignmentData >> 4) & 0xF) / 16.0f;
                float scrollD = (texture_for_block.alignmentData & 0xF) / 16.0f;

                // TODO: Use textures alignment data to modify these UV's

                //FLIP SHIT HERE
                uvs.emplace_back(1.0f, 1.0f); //0
                uvs.emplace_back(0.0f, 1.0f); //1
                uvs.emplace_back(0.0f, 0.0f); //2
                uvs.emplace_back(1.0f, 1.0f); //0
                uvs.emplace_back(0.0f, 0.0f); //2
                uvs.emplace_back(1.0f, 0.0f); //3


                texture_indices.emplace_back(texture_for_block.texNumber);
                texture_indices.emplace_back(texture_for_block.texNumber);
                texture_indices.emplace_back(texture_for_block.texNumber);
                texture_indices.emplace_back(texture_for_block.texNumber);
                texture_indices.emplace_back(texture_for_block.texNumber);
                texture_indices.emplace_back(texture_for_block.texNumber);
                // TODO: Calculate normals properly
                norms.emplace_back(glm::vec3(1, 1, 1));
                norms.emplace_back(glm::vec3(1, 1, 1));
                norms.emplace_back(glm::vec3(1, 1, 1));
                norms.emplace_back(glm::vec3(1, 1, 1));
                norms.emplace_back(glm::vec3(1, 1, 1));
                norms.emplace_back(glm::vec3(1, 1, 1));
            }
            // Get ordered list of unique texture id's present in block
            std::vector<short> texture_ids = TrackUtils::RemapTextureIDs(minimal_texture_ids_set, texture_indices);
            Track current_trk_block_model = Track("TrkBlock", trkBlock.header->blockSerial, verts, uvs, texture_indices,
                                                  vertex_indices,
                                                  texture_ids,
                                                  trk_block_shading_verts,
                                                  trk_block_center);
            current_trk_block_model.enable();
            //current_track_block.track.emplace_back(current_trk_block_model);
            current_track_block.objects.emplace_back(current_trk_block_model);

            track->track_blocks.emplace_back(current_track_block);
        }

    }
}

template<typename Platform>
std::vector<Track> NFS2<Platform>::ParseCOLModels(shared_ptr<typename Platform::TRACK> track) {
    std::vector<Track> col_models;

    // Parse out COL data
    for (int structure_Idx = 0; structure_Idx < track->nColStructures; ++structure_Idx) {
        VERT_HIGHP *structureReferenceCoordinates = static_cast<VERT_HIGHP *>(calloc(1, sizeof(VERT_HIGHP)));

        std::set<short> minimal_texture_ids_set;
        std::vector<unsigned int> indices;
        std::vector<glm::vec2> uvs;
        std::vector<unsigned int> texture_indices;
        std::vector<glm::vec3> verts;
        std::vector<glm::vec4> shading_data;

        // Find the structure reference that matches this structure, else use block default
        for (int structRef_Idx = 0; structRef_Idx < track->nColStructureReferences; ++structRef_Idx) {
            // Only check fixed type structure references
            if (track->colStructureRefData[structRef_Idx].structureRef == structure_Idx) {
                if (track->colStructureRefData[structRef_Idx].recType == 1 ||
                    track->colStructureRefData[structRef_Idx].recType == 4) {
                    structureReferenceCoordinates = &track->colStructureRefData[structure_Idx].refCoordinates;
                    break;
                } else if (track->colStructureRefData[structRef_Idx].recType == 3) {
                    //if(track->colStructureRefData[structure_Idx].animLength != 0){
                    // For now, if animated, use position 0 of animation sequence
                    structureReferenceCoordinates = &track->colStructureRefData[structure_Idx].animationData[0].position;
                    break;
                    //}
                }
            }
            //if (structRef_Idx == track->nColStructureReferences - 1)
            //std::cout << "Couldn't find a reference coordinate for COL Structure " << structRef_Idx << std::endl;
        }
        for (uint16_t vert_Idx = 0; vert_Idx < track->colStructures[structure_Idx].nVerts; ++vert_Idx) {
            int32_t x = (structureReferenceCoordinates->x +
                         (256 * track->colStructures[structure_Idx].vertexTable[vert_Idx].x));
            int32_t y = (structureReferenceCoordinates->y +
                         (256 * track->colStructures[structure_Idx].vertexTable[vert_Idx].y));
            int32_t z = (structureReferenceCoordinates->z +
                         (256 * track->colStructures[structure_Idx].vertexTable[vert_Idx].z));
            verts.emplace_back(glm::vec3(x / scaleFactor,
                                         z / scaleFactor,
                                         y / scaleFactor));
            shading_data.emplace_back(glm::vec4(1.0, 1.0f, 1.0f, 1.0f));
        }

        for (int poly_Idx = 0; poly_Idx < track->colStructures[structure_Idx].nPoly; ++poly_Idx) {
            // Remap the COL TextureID's using the COL texture block (XBID2)
            TEXTURE_BLOCK texture_for_block = track->polyToQFStexTable[track->colStructures[structure_Idx].polygonTable[poly_Idx].texture];
            minimal_texture_ids_set.insert(texture_for_block.texNumber);
            indices.emplace_back(track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[0]);
            indices.emplace_back(track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[1]);
            indices.emplace_back(track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[2]);
            indices.emplace_back(track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[0]);
            indices.emplace_back(track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[2]);
            indices.emplace_back(track->colStructures[structure_Idx].polygonTable[poly_Idx].vertex[3]);
            // TODO: Use textures alignment data to modify these UV's
            uvs.emplace_back(1.0f, 1.0f);
            uvs.emplace_back(0.0f, 1.0f);
            uvs.emplace_back(0.0f, 0.0f);
            uvs.emplace_back(1.0f, 1.0f);
            uvs.emplace_back(0.0f, 0.0f);
            uvs.emplace_back(1.0f, 0.0f);
            texture_indices.emplace_back(texture_for_block.texNumber);
            texture_indices.emplace_back(texture_for_block.texNumber);
            texture_indices.emplace_back(texture_for_block.texNumber);
            texture_indices.emplace_back(texture_for_block.texNumber);
            texture_indices.emplace_back(texture_for_block.texNumber);
            texture_indices.emplace_back(texture_for_block.texNumber);
        }
        // Get ordered list of unique texture id's present in block
        std::vector<short> texture_ids = TrackUtils::RemapTextureIDs(minimal_texture_ids_set, texture_indices);
        glm::vec3 position = glm::vec3(0, 0, 0);
        Track col_model = Track("ColBlock", structure_Idx, verts, uvs, texture_indices, indices, texture_ids,
                                shading_data, glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0))) * position);
        col_model.enable();
        col_models.emplace_back(col_model);
        free(structureReferenceCoordinates);
    }
    return col_models;
}

template<typename Platform>
Texture
NFS2<Platform>::LoadTexture(TEXTURE_BLOCK track_texture, const std::string &track_name, NFSVer nfs_version) {
    std::stringstream filename;
    uint8_t alphaColour = 0;
    filename << TRACK_PATH;

    switch (nfs_version) {
        case NFS_2:
            alphaColour = 0u;
            filename << "NFS2/";
            break;
        case NFS_2_SE:
            alphaColour = 248u;
            filename << "NFS2_SE/";
            break;
        case NFS_3_PS1:
            filename << "NFS3_PS1/";
            break;
        case UNKNOWN:
        default:
            filename << "UNKNOWN/";
            break;
    }
    filename << track_name << "/textures/" << setfill('0') << setw(4) << track_texture.texNumber << ".BMP";;

    GLubyte *data;
    GLsizei width;
    GLsizei height;

    ASSERT(Utils::LoadBmpCustomAlpha(filename.str().c_str(), &data, &width, &height, alphaColour),
           "Texture " << filename.str() << " did not load succesfully!");

    return Texture((unsigned int) track_texture.texNumber, data, static_cast<unsigned int>(width),
                   static_cast<unsigned int>(height));
}

template
class NFS2<PS1>;

template
class NFS2<PC>;