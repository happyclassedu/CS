#ifndef __CS_AWS_TEXTURE_MANAGER_H__
#define __CS_AWS_TEXTURE_MANAGER_H__

# include "csutil/parray.h"

struct iString;
struct iObjectRegistry;
struct iReporter;
struct iTextureHandle;
struct iImageIO;
struct iVFS;
struct iImage;

/**
 *
 *  This class embeds a normal texture manager, and keeps track of all the textures currently
 * in use by the windowing system.  This includes bitmaps for buttons, etc.  When the skin
 * changes, it unloads all the skin textures currently being used.  Then it is ready to demand-load
 * new ones.
 *
 */
class awsTextureManager
{
  /// this contains a reference to our loader.
  csRef<iImageIO> loader;

  /// this contains a reference to our texture manager
  csRef<iTextureManager> txtmgr;

  /// this contains a reference to the VFS plugin
  csRef<iVFS> vfs;

  /// contains a reference to the object registry
  iObjectRegistry *object_reg;

  struct awsTexture
  {
    ~awsTexture();
    csRef<iImage> img;
    csRef<iTextureHandle> tex;
    unsigned long id;
  };

  /// list of textures loaded
  csPDelArray<awsTexture> textures;

private:
  /// registers all currently loaded textures with the texture manager
  void RegisterTextures ();

  /// unregisters all currently loaded textures with the texture manager
  void UnregisterTextures ();
public:
  /// empty constructor
  awsTextureManager ();

  /// de-inits
  ~awsTextureManager ();

  /** Get's a reference to and iLoader. */
  void Initialize (iObjectRegistry *object_reg);

  /** Get's a texture.  If the texture is already cached, it returns the cached texture.
   * If the texture has not been cached, and a filename is specified, the file is loaded.
   * If the file cannot be found, or no file was specified, 0 is returned. */
  iTextureHandle *GetTexture (
                    const char *name,
                    const char *filename = 0,
                    bool replace = false,
                    unsigned char key_r = 255,
                    unsigned char key_g = 0,
                    unsigned char key_b = 255
                    );

  /** Get's a texture.  If the texture is already cached, it returns the cached texture.
  * If the texture has not been cached, and a filename is specified, the file is loaded.
  * If the file cannot be found, or no file was specified, 0 is returned. This variety
    uses the id directly, in case you have it.  Mostly used internally by AWSPrefManager. */
  iTextureHandle *GetTexturebyID (
                    unsigned long id,
                    const char *filename = 0,
                    bool replace = false,
                    unsigned char key_r = 255,
                    unsigned char key_g = 0,
                    unsigned char key_b = 255
                    );

  /** Changes the texture manager: unregisters all current textures, and then re-registers them
   * with the new manager */
  void SetTextureManager (iTextureManager *txtmgr);

  /** Retrieves the texture manager that we are currently using */
  iTextureManager *GetTextureManager () { return txtmgr; }
};
#endif // __CS_AWS_TEXTURE_MANAGER_H__
