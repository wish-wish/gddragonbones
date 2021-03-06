#include "gddragonbones.h"
#include "core/io/resource_loader.h"

#include "core/globals.h"
#include "core/os/file_access.h"
#include "core/os/os.h"

#include "method_bind_ext.inc"

//////////////////////////////////////////////////////////////////
//// Resource
GDDragonBones::GDDragonBonesResource::GDDragonBonesResource()
{
    p_data_texture_atlas = nullptr;
    p_data_bones = nullptr;
}

GDDragonBones::GDDragonBonesResource::~GDDragonBonesResource()
{
    if(p_data_texture_atlas)
    {
        memfree(p_data_texture_atlas);
        p_data_texture_atlas = nullptr;
    }

    if(p_data_bones)
    {
        memfree(p_data_bones);
        p_data_bones = nullptr;
    }
}

char*    __load_file(const String& _file_path)
{
    FileAccess* __p_f = FileAccess::open(_file_path, FileAccess::READ);
    ERR_FAIL_COND_V(!__p_f, nullptr);
    ERR_FAIL_COND_V(!__p_f->get_len(), nullptr);

   // mem
    char* __p_data = (char*)memalloc(__p_f->get_len() + 1);
    ERR_FAIL_COND_V(!__p_data, nullptr);

    __p_f->get_buffer((uint8_t *)__p_data, __p_f->get_len());
    __p_data[__p_f->get_len()] = 0x00;
  
    memdelete(__p_f);

    return __p_data;
}

void       GDDragonBones::GDDragonBonesResource::set_def_texture_path(const String& _path)
{
    str_default_tex_path = _path;
}

bool       GDDragonBones::GDDragonBonesResource::load_texture_atlas_data(const String& _path)
{
    p_data_texture_atlas = __load_file(_path);
    ERR_FAIL_COND_V(!p_data_texture_atlas, false);
    return true;
}

bool       GDDragonBones::GDDragonBonesResource::load_bones_data(const String& _path)
{
    p_data_bones = __load_file(_path);
    ERR_FAIL_COND_V(!p_data_bones, false);
    return true;
}

/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
//// Plugin module
GDDragonBones::GDDragonBones()
{
    p_factory = memnew(GDFactory(this));

    m_res = RES();
    str_curr_anim = "[none]";
    p_armature = nullptr;
    m_anim_mode = ANIMATION_PROCESS_IDLE;
    f_speed = 1.f;
    f_bone_opacity = 1.f;
    b_processing = false;
    b_active = true;
    b_playing = false;
    b_debug = false;
    c_loop = -1;
    b_inited = false;
}

GDDragonBones::~GDDragonBones()
{
    _cleanup();

    if(p_factory)
    {
      memdelete(p_factory);
      p_factory = nullptr;
    }
}

void GDDragonBones::_cleanup()
{
    b_inited = false;

    if(p_factory)
        p_factory->clear();

    if(p_armature)
    {
	if (p_armature->is_inside_tree())
		remove_child(p_armature);
        p_armature = nullptr;
    }

    m_res = RES();
}

void GDDragonBones::dispatch_snd_event(const String& _str_type, const EventObject* _p_value)
{
    if(get_tree()->is_editor_hint())
	return;

   if(_str_type == EventObject::SOUND_EVENT)
       emit_signal("dragon_anim_snd_event", String(_p_value->animationState->name.c_str()), String(_p_value->name.c_str()));
}

void GDDragonBones::dispatch_event(const String& _str_type, const EventObject* _p_value)
{
    if(get_tree()->is_editor_hint())
	return;

    if(_str_type == EventObject::START)
        emit_signal("dragon_anim_start", String(_p_value->animationState->name.c_str()));
    else if(_str_type == EventObject::LOOP_COMPLETE)
        emit_signal("dragon_anim_loop_complete", String(_p_value->animationState->name.c_str()));
    else if(_str_type == EventObject::COMPLETE)
        emit_signal("dragon_anim_complete", String(_p_value->animationState->name.c_str()));
    else if(_str_type == EventObject::FRAME_EVENT)
        emit_signal("dragon_anim_event", String(_p_value->animationState->name.c_str()), String(_p_value->name.c_str()));
    else if(_str_type == EventObject::FADE_IN)
        emit_signal("dragon_fade_in", String(_p_value->animationState->name.c_str()));
    else if(_str_type == EventObject::FADE_IN_COMPLETE)
        emit_signal("dragon_fade_in_complete", String(_p_value->animationState->name.c_str()));
    else if(_str_type == EventObject::FADE_OUT)
        emit_signal("dragon_fade_out", String(_p_value->animationState->name.c_str()));
    else if(_str_type == EventObject::FADE_OUT_COMPLETE)
        emit_signal("dragon_fade_out_complete", String(_p_value->animationState->name.c_str()));

}

void GDDragonBones::set_resource(Ref<GDDragonBones::GDDragonBonesResource> _p_data)
{
    String __old_texture_path = "";
    if(m_res.is_valid())
        __old_texture_path = m_res->str_default_tex_path;
   else
        __old_texture_path = _p_data->str_default_tex_path;

    if (m_res == _p_data)
		return;

    bool __b_pl = b_playing;
    stop();

    _cleanup();

    m_res = _p_data;
    if (m_res.is_null())
    {
        ERR_PRINT("Null resources");
		return;
    }

    ERR_FAIL_COND(!m_res->p_data_texture_atlas);
    ERR_FAIL_COND(!m_res->p_data_bones);

    TextureAtlasData* __p_tad = p_factory->loadTextureAtlasData(m_res->p_data_texture_atlas, nullptr);
    ERR_FAIL_COND(!__p_tad);
    DragonBonesData* __p_dbd = p_factory->loadDragonBonesData(m_res->p_data_bones);
    ERR_FAIL_COND(!__p_dbd);

    // build Armature display
    const std::vector<std::string>& __r_v_m_names = __p_dbd->getArmatureNames();
    ERR_FAIL_COND(!__r_v_m_names.size());

    p_armature = static_cast<GDArmatureDisplay*>(p_factory->buildArmatureDisplay(__r_v_m_names[0].c_str()));
    // add children armature
    p_armature->p_owner = this;

    if(!m_texture_atlas.is_valid() || __old_texture_path != m_res->str_default_tex_path)
        m_texture_atlas = ResourceLoader::load(m_res->str_default_tex_path);

    p_armature->add_parent_class(b_debug, m_texture_atlas);
    // add main armature
    add_child(p_armature);

    b_inited = true;

    _change_notify();
    update();
}

Ref<GDDragonBones::GDDragonBonesResource> GDDragonBones::get_resource()
{
    return m_res;
}

void GDDragonBones::set_opacity(float _f_opacity)
{
    f_bone_opacity = _f_opacity;
    GDOwnerNode::set_opacity(f_bone_opacity);
    if(p_armature)
        p_armature->update_child_colors();
}

float GDDragonBones::get_opacity() const
{
    return f_bone_opacity;
}

void GDDragonBones::fade_in(const String& _name_anim, float _time, int _loop, int _layer, const String& _group, GDDragonBones::AnimFadeOutMode _fade_out_mode)
{
    // setup speed
    p_factory->set_speed(f_speed);
    if(has_anim(_name_anim))
    {
        p_armature->getAnimation()->fadeIn(_name_anim.ascii().get_data(), _time, _loop, _layer, _group.ascii().get_data(), (AnimationFadeOutMode)_fade_out_mode);
	if(!b_playing)
	{
		b_playing = true;
        	_set_process(true);
	}
    }
}

void GDDragonBones::set_modulate(const Color& p_color)
{
    modulate = p_color;
    if(p_armature)
        p_armature->update_child_colors();
    _change_notify();
    update();
}

Color GDDragonBones::get_modulate() const
{
    return modulate;
}

void GDDragonBones::set_active(bool _b_active)
{
    if (b_active == _b_active)  return;
    b_active = _b_active;
    _set_process(b_processing, true);
}

bool GDDragonBones::is_active() const
{
    return b_active;
}

void GDDragonBones::set_debug(bool _b_debug)
{
    b_debug = _b_debug;
    if(b_inited)
        p_armature->set_debug(b_debug);
}

bool GDDragonBones::is_debug() const
{
    return b_debug;
}


void GDDragonBones::set_speed(float _f_speed)
{
    f_speed = _f_speed;
    if(b_inited)
        p_factory->set_speed(_f_speed);
}

float GDDragonBones::get_speed() const
{
    return f_speed;
}

void GDDragonBones::set_animation_process_mode(GDDragonBones::AnimMode _mode)
{
    if (m_anim_mode == _mode)
        return;
    bool pr = b_processing;
    if (pr)
        _set_process(false);
    m_anim_mode = _mode;
    if (pr)
        _set_process(true);
}

GDDragonBones::AnimMode GDDragonBones::get_animation_process_mode() const
{
    return m_anim_mode;
}

void GDDragonBones::_notification(int p_what)
{
    switch (p_what)
    {
        case NOTIFICATION_ENTER_TREE:
        {
            if (!b_processing)
            {
                set_fixed_process(false);
                set_process(false);
            }
        }
        break;

        case NOTIFICATION_READY:
        {
            if (b_playing && b_inited)
                play();
        }
        break;


        case NOTIFICATION_PROCESS:
        {
            if (m_anim_mode == ANIMATION_PROCESS_FIXED)
                break;

            if (b_processing)
                p_factory->update(get_process_delta_time());
        }
        break;

        case NOTIFICATION_FIXED_PROCESS:
        {

            if (m_anim_mode == ANIMATION_PROCESS_IDLE)
                break;

            if (b_processing)
                p_factory->update(get_fixed_process_delta_time());
        }
        break;

        case NOTIFICATION_EXIT_TREE:
        {
            //stop_all();
        }
        break;
    }
}

void    GDDragonBones::_reset()
{
    p_armature->getAnimation()->reset();
}

void   GDDragonBones::play(bool _b_play)
{
    b_playing = _b_play;
    if(!_b_play)
    {
        stop();
        return;
    }
    // setup speed
    p_factory->set_speed(f_speed);
    if(has_anim(str_curr_anim))
    {
        p_armature->getAnimation()->play(str_curr_anim.ascii().get_data(), c_loop);
        _set_process(true);
    } else
    {
        str_curr_anim  = "[none]";
        stop();
    }
}

bool GDDragonBones::has_anim(const String& _str_anim)
{
    return p_armature->getAnimation()->hasAnimation(_str_anim.ascii().get_data());
}

void GDDragonBones::stop_all()
{
    stop();
}

void GDDragonBones::stop()
{
    if(!b_inited) return;

    _set_process(false);
    b_playing = false;

    if(p_armature->getAnimation()->isPlaying())
        p_armature->getAnimation()->stop(str_curr_anim.ascii().get_data());

    _reset();
    _change_notify();
}

float GDDragonBones::tell() const
{
    if(b_inited && str_curr_anim != "[none]" && p_armature->getAnimation())
    {
        AnimationState* __p_state = p_armature->getAnimation()->getState(str_curr_anim.ascii().get_data());
        if(__p_state)
            return __p_state->getCurrentTime()/__p_state->_duration;
    }
    return 0;
}

void GDDragonBones::seek(float _f_p)
{
    if(b_inited && str_curr_anim != "[none]" && p_armature->getAnimation())
    {
        stop();
        p_armature->getAnimation()->gotoAndStopByProgress(str_curr_anim.ascii().get_data(), _f_p);
    }
}

bool GDDragonBones::is_playing() const
{
    return b_inited && b_playing && p_armature->getAnimation()->isPlaying();
}

String GDDragonBones::get_current_animation() const
{
    if(!b_inited || !p_armature->getAnimation())
        return String("");
    return String(p_armature->getAnimation()->getLastAnimationName().c_str());
}

void GDDragonBones::_set_process(bool _b_process, bool _b_force)
{
    if (b_processing == _b_process && !_b_force)
        return;

    switch (m_anim_mode)
    {
        case ANIMATION_PROCESS_FIXED: set_fixed_process(_b_process && b_active); break;
        case ANIMATION_PROCESS_IDLE: set_process(_b_process && b_active); break;
    }
    b_processing = _b_process;
}

void GDDragonBones::set_texture(const Ref<Texture> &p_texture) {

    if (p_texture == m_texture_atlas)
        return;

    m_texture_atlas = p_texture;

#ifdef DEBUG_ENABLED
    if (m_texture_atlas.is_valid()) {
        m_texture_atlas->set_flags(m_texture_atlas->get_flags()); //remove repeat from texture, it looks bad in sprites
//        m_texture_atlas->connect(CoreStringNames::get_singleton()->changed, this, SceneStringNames::get_singleton()->update);
    }
#endif

    if(p_armature)
    {
        p_armature->update_texture_atlas(m_texture_atlas);
        update();
    }
}

Ref<Texture> GDDragonBones::get_texture() const {

    return m_texture_atlas;
}

bool GDDragonBones::_set(const StringName& _str_name, const Variant& _c_r_value)
{
    String name = _str_name;

    if (name == "playback/curr_animation")
    {
        if(str_curr_anim == _c_r_value)
            return false;

        str_curr_anim = _c_r_value;
        if (b_inited)
        {
            if (str_curr_anim == "[none]")
                stop();
            else if (has_anim(str_curr_anim))
            {
                if(b_playing)
                    play();
                else
                    p_armature->getAnimation()->gotoAndStopByProgress(str_curr_anim.ascii().get_data());
            }
        }
    }

   else if (name == "playback/loop")
   {
        c_loop = _c_r_value;
        if (b_inited && b_playing)
        {
            _reset();
            play();
        }
    }

    else if (name == "playback/progress")
    {
       if (b_inited && has_anim(str_curr_anim))
       {
           seek(_c_r_value);
       }
    }
    return true;
}

bool GDDragonBones::_get(const StringName& _str_name, Variant &_r_ret) const
{

    String __name = _str_name;

    if (__name == "playback/curr_animation")
        _r_ret = str_curr_anim;
    else if (__name == "playback/loop")
        _r_ret = c_loop;
    else if (__name == "playback/progress")
        _r_ret = tell();
    return true;
}

void GDDragonBones::_bind_methods()
{

    ObjectTypeDB::bind_method(_MD("set_texture", "texture:Texture"), &GDDragonBones::set_texture);
    ObjectTypeDB::bind_method(_MD("get_texture:Texture"), &GDDragonBones::get_texture);

    ObjectTypeDB::bind_method(_MD("set_resource", "dragonbones"), &GDDragonBones::set_resource);
    ObjectTypeDB::bind_method(_MD("get_resource"), &GDDragonBones::get_resource);

    ObjectTypeDB::bind_method(_MD("set_modulate", "modulate"), &GDDragonBones::set_modulate);
    ObjectTypeDB::bind_method(_MD("get_modulate"), &GDDragonBones::get_modulate);

    ObjectTypeDB::bind_method(_MD("set_opacity", "opacity"), &GDDragonBones::set_opacity);
    ObjectTypeDB::bind_method(_MD("get_opacity"), &GDDragonBones::get_opacity);

    ObjectTypeDB::bind_method(_MD("fade_in", "anim_name", "time", "loop", "layer", "group", "fade_out_mode"), &GDDragonBones::fade_in);

    ObjectTypeDB::bind_method(_MD("stop"), &GDDragonBones::stop);
    ObjectTypeDB::bind_method(_MD("reset"), &GDDragonBones::_reset);
    ObjectTypeDB::bind_method(_MD("play"), &GDDragonBones::play);
    ObjectTypeDB::bind_method(_MD("has", "name"), &GDDragonBones::has_anim);
    ObjectTypeDB::bind_method(_MD("is_playing"), &GDDragonBones::is_playing);

    ObjectTypeDB::bind_method(_MD("get_current_animation"), &GDDragonBones::get_current_animation);

    ObjectTypeDB::bind_method(_MD("seek", "pos"), &GDDragonBones::seek);
    ObjectTypeDB::bind_method(_MD("tell"), &GDDragonBones::tell);

    ObjectTypeDB::bind_method(_MD("set_active", "active"), &GDDragonBones::set_active);
    ObjectTypeDB::bind_method(_MD("is_active"), &GDDragonBones::is_active);

    ObjectTypeDB::bind_method(_MD("set_debug", "debug"), &GDDragonBones::set_debug);
    ObjectTypeDB::bind_method(_MD("is_debug"), &GDDragonBones::is_debug);

    ObjectTypeDB::bind_method(_MD("set_speed", "speed"), &GDDragonBones::set_speed);
    ObjectTypeDB::bind_method(_MD("get_speed"), &GDDragonBones::get_speed);

    ObjectTypeDB::bind_method(_MD("set_animation_process_mode","mode"),&GDDragonBones::set_animation_process_mode);
    ObjectTypeDB::bind_method(_MD("get_animation_process_mode"),&GDDragonBones::get_animation_process_mode);

    ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), _SCS("set_texture"), _SCS("get_texture"));

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug"), _SCS("set_debug"), _SCS("is_debug"));

    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "modulate"), _SCS("set_modulate"), _SCS("get_modulate"));
    ADD_PROPERTYNO(PropertyInfo(Variant::REAL, "visibility/BonesOpacity", PROPERTY_HINT_RANGE, "0,1,0.01"), _SCS("set_opacity"), _SCS("get_opacity"));

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "GDDragonBonesResource"), _SCS("set_resource"), _SCS("get_resource"));

    ADD_PROPERTY(PropertyInfo(Variant::INT, "playback/process_mode", PROPERTY_HINT_ENUM, "Fixed,Idle"), _SCS("set_animation_process_mode"), _SCS("get_animation_process_mode"));
    ADD_PROPERTY(PropertyInfo(Variant::REAL, "playback/speed", PROPERTY_HINT_RANGE, "0,10,0.01"), _SCS("set_speed"), _SCS("get_speed"));
    ADD_PROPERTY(PropertyInfo(Variant::REAL, "playback/progress", PROPERTY_HINT_RANGE, "0,1,0.010"), _SCS("seek"), _SCS("tell"));
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playback/play"), _SCS("play"), _SCS("is_playing"));

    ADD_SIGNAL(MethodInfo("dragon_anim_start", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_anim_complete", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_anim_event", PropertyInfo(Variant::STRING, "anim"), PropertyInfo(Variant::STRING, "ev")));
    ADD_SIGNAL(MethodInfo("dragon_anim_loop_complete", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_anim_snd_event", PropertyInfo(Variant::STRING, "anim"), PropertyInfo(Variant::STRING, "ev")));
    ADD_SIGNAL(MethodInfo("dragon_fade_in", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_fade_in_complete", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_fade_out", PropertyInfo(Variant::STRING, "anim")));
    ADD_SIGNAL(MethodInfo("dragon_fade_out_complete", PropertyInfo(Variant::STRING, "anim")));

    BIND_CONSTANT(ANIMATION_PROCESS_FIXED);
    BIND_CONSTANT(ANIMATION_PROCESS_IDLE);

    BIND_CONSTANT(FadeOut_None);
    BIND_CONSTANT(FadeOut_SameLayer);
    BIND_CONSTANT(FadeOut_SameGroup);
    BIND_CONSTANT(FadeOut_SameLayerAndGroup);
    BIND_CONSTANT(FadeOut_All);
    BIND_CONSTANT(FadeOut_Single);
}

void GDDragonBones::_get_property_list(List<PropertyInfo> *p_list) const
{
    List<String> names;

    if (b_inited && p_armature->getAnimation())
    {
        auto __names = p_armature->getAnimation()->getAnimationNames();
        auto __it = __names.cbegin();
        while(__it != __names.cend())
        {
            names.push_back(__it->c_str());
            ++__it;
        }
    }

    {
        names.sort();
        names.push_front("[none]");
        String hint;
        for (List<String>::Element *E = names.front(); E; E = E->next()) {

            if (E != names.front())
                hint += ",";
            hint += E->get();
        }

        p_list->push_back(PropertyInfo(Variant::STRING, "playback/curr_animation", PROPERTY_HINT_ENUM, hint));

        p_list->push_back(PropertyInfo(Variant::INT, "playback/loop", PROPERTY_HINT_RANGE, "-1,100,1"));
    }
}
